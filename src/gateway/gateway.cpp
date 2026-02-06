#include "gateway/gateway.h"
#include "gateway/udp_socket.h"
#include "replay/replay_engine.h"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace nng {

Gateway::Gateway(const GatewayConfig& config)
    : config_(config) {
    Logger::instance().set_level(config_.log_level);
}

Gateway::~Gateway() {
    stop();
}

void Gateway::run() {
    if (running_.load())
        return;

    // Create frame source
    if (!config_.replay_path.empty()) {
        // Replay mode
        auto replay = std::make_unique<ReplayFrameSource>();
        if (!replay->open(config_.replay_path)) {
            Logger::instance().log(Severity::ERROR, EventCategory::NETWORK,
                "EVT_SOURCE_TIMEOUT", "Failed to open replay file: " + config_.replay_path);
            return;
        }
        replay->set_speed(0.0); // As fast as possible for processing
        source_ = std::move(replay);
    } else {
        // UDP mode
        auto udp = std::make_unique<UdpFrameSource>();
        if (!udp->bind(config_.udp_port)) {
            Logger::instance().log(Severity::ERROR, EventCategory::NETWORK,
                "EVT_SOURCE_TIMEOUT", "Failed to bind UDP port " + std::to_string(config_.udp_port));
            return;
        }
        udp->set_timeout_ms(100);
        source_ = std::move(udp);
    }

    // Open recorder if enabled
    if (config_.record_enabled) {
        if (!recorder_.open(config_.record_path)) {
            Logger::instance().log(Severity::WARN, EventCategory::NETWORK,
                "EVT_CONFIG_CHANGE", "Failed to open record file: " + config_.record_path);
        }
    }

    running_.store(true);
    should_stop_.store(false);

    Logger::instance().log(Severity::INFO, EventCategory::CONTROL,
        "EVT_CONFIG_CHANGE", "Gateway started on port " + std::to_string(config_.udp_port));

    std::vector<uint8_t> buf;
    while (!should_stop_.load()) {
        if (!source_->receive(buf)) {
            // Check if replay is done
            if (!config_.replay_path.empty()) {
                auto* replay = dynamic_cast<ReplayFrameSource*>(source_.get());
                if (replay && replay->is_done()) {
                    break;
                }
            }
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        uint64_t rx_timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();

        process_frame(buf, rx_timestamp_ns);
    }

    // Close recorder
    if (config_.record_enabled) {
        recorder_.close();
    }

    running_.store(false);

    Logger::instance().log(Severity::INFO, EventCategory::CONTROL,
        "EVT_CONFIG_CHANGE", "Gateway stopped");
}

void Gateway::stop() {
    should_stop_.store(true);
}

void Gateway::process_frame(const std::vector<uint8_t>& frame, uint64_t rx_timestamp_ns) {
    // Record if enabled
    if (config_.record_enabled && recorder_.is_open()) {
        recorder_.record(rx_timestamp_ns, frame.data(), frame.size());
    }

    // Parse frame
    ParsedFrame parsed;
    auto err = parse_frame(frame.data(), frame.size(), config_.crc_enabled, parsed);

    if (err != ParseError::OK) {
        stats_.record_malformed(0);

        std::string error_str;
        switch (err) {
            case ParseError::TOO_SHORT:      error_str = "TOO_SHORT"; break;
            case ParseError::BAD_VERSION:    error_str = "BAD_VERSION"; break;
            case ParseError::BAD_MSG_TYPE:   error_str = "BAD_MSG_TYPE"; break;
            case ParseError::PAYLOAD_TOO_LONG: error_str = "PAYLOAD_TOO_LONG"; break;
            case ParseError::TRUNCATED:      error_str = "TRUNCATED"; break;
            case ParseError::CRC_MISMATCH:   error_str = "CRC_MISMATCH"; break;
            default:                         error_str = "UNKNOWN"; break;
        }

        if (err == ParseError::CRC_MISMATCH) {
            stats_.record_crc_fail(0);
            publish_event(EventId::EVT_CRC_FAIL, EventCategory::NETWORK,
                Severity::WARN, "error=" + error_str);
        } else {
            publish_event(EventId::EVT_FRAME_MALFORMED, EventCategory::NETWORK,
                Severity::WARN, "error=" + error_str + " len=" + std::to_string(frame.size()));
        }
        return;
    }

    // Track sequence
    auto seq_result = tracker_.track(parsed.header.src_id, parsed.header.seq);

    // Record stats
    stats_.record_rx(parsed.header.src_id, parsed.header.seq, rx_timestamp_ns);

    // Handle sequence anomalies
    switch (seq_result.result) {
        case SeqResult::FIRST:
            publish_event(EventId::EVT_SOURCE_ONLINE, EventCategory::NETWORK,
                Severity::INFO, "src_id=" + std::to_string(parsed.header.src_id));
            break;

        case SeqResult::GAP:
            stats_.record_gap(parsed.header.src_id, seq_result.gap_size);
            publish_event(EventId::EVT_SEQ_GAP, EventCategory::NETWORK,
                Severity::WARN, "src_id=" + std::to_string(parsed.header.src_id) +
                " expected=" + std::to_string(seq_result.expected_seq) +
                " actual=" + std::to_string(seq_result.actual_seq) +
                " gap=" + std::to_string(seq_result.gap_size));
            break;

        case SeqResult::REORDER:
            stats_.record_reorder(parsed.header.src_id);
            publish_event(EventId::EVT_SEQ_REORDER, EventCategory::NETWORK,
                Severity::WARN, "src_id=" + std::to_string(parsed.header.src_id) +
                " expected=" + std::to_string(seq_result.expected_seq) +
                " actual=" + std::to_string(seq_result.actual_seq));
            break;

        case SeqResult::DUPLICATE:
            stats_.record_duplicate(parsed.header.src_id);
            break;

        case SeqResult::OK:
            // Normal frame, no special logging needed
            break;
    }

    // Process by message type
    MsgType msg_type = static_cast<MsgType>(parsed.header.msg_type);
    switch (msg_type) {
        case MsgType::TRACK: {
            if (parsed.header.payload_len >= sizeof(TrackPayload)) {
                TrackPayload track = deserialize_track(parsed.payload_ptr);
                std::ostringstream detail;
                detail << "src_id=" << parsed.header.src_id
                       << " track_id=" << track.track_id
                       << " class=" << static_cast<int>(track.classification)
                       << " threat=" << static_cast<int>(track.threat_level);
                publish_event(EventId::EVT_TRACK_UPDATE, EventCategory::TRACKING,
                    Severity::DEBUG, detail.str());
            }
            break;
        }

        case MsgType::PLOT: {
            if (parsed.header.payload_len >= sizeof(PlotPayload)) {
                PlotPayload plot = deserialize_plot(parsed.payload_ptr);
                std::ostringstream detail;
                detail << "src_id=" << parsed.header.src_id
                       << " plot_id=" << plot.plot_id
                       << " range=" << plot.range_m << "m";
                publish_event(EventId::EVT_TRACK_NEW, EventCategory::TRACKING,
                    Severity::DEBUG, detail.str());
            }
            break;
        }

        case MsgType::HEARTBEAT: {
            if (parsed.header.payload_len >= sizeof(HeartbeatPayload)) {
                HeartbeatPayload hb = deserialize_heartbeat(parsed.payload_ptr);
                SubsystemState state = static_cast<SubsystemState>(hb.state);

                EventId evt_id = EventId::EVT_HEARTBEAT_OK;
                Severity sev = Severity::DEBUG;
                if (state == SubsystemState::DEGRADED) {
                    evt_id = EventId::EVT_HEARTBEAT_DEGRADE;
                    sev = Severity::WARN;
                } else if (state == SubsystemState::ERROR || state == SubsystemState::OFFLINE) {
                    evt_id = EventId::EVT_HEARTBEAT_ERROR;
                    sev = Severity::ALARM;
                }

                std::ostringstream detail;
                detail << "subsystem=" << hb.subsystem_id
                       << " state=" << static_cast<int>(hb.state)
                       << " cpu=" << static_cast<int>(hb.cpu_pct) << "%"
                       << " mem=" << static_cast<int>(hb.mem_pct) << "%";
                publish_event(evt_id, EventCategory::HEALTH, sev, detail.str());
            }
            break;
        }

        case MsgType::ENGAGEMENT: {
            if (parsed.header.payload_len >= sizeof(EngagementPayload)) {
                EngagementPayload eng = deserialize_engagement(parsed.payload_ptr);
                std::ostringstream detail;
                detail << "weapon=" << eng.weapon_id
                       << " mode=" << static_cast<int>(eng.mode)
                       << " track=" << eng.assigned_track
                       << " rounds=" << eng.rounds_remaining;
                publish_event(EventId::EVT_WEAPON_STATUS, EventCategory::ENGAGEMENT,
                    Severity::INFO, detail.str());
            }
            break;
        }
    }
}

void Gateway::publish_event(EventId id, EventCategory cat, Severity sev, const std::string& detail) {
    // Log
    const char* event_name = "UNKNOWN";
    switch (id) {
        case EventId::EVT_TRACK_NEW:        event_name = "EVT_TRACK_NEW"; break;
        case EventId::EVT_TRACK_UPDATE:     event_name = "EVT_TRACK_UPDATE"; break;
        case EventId::EVT_TRACK_LOST:       event_name = "EVT_TRACK_LOST"; break;
        case EventId::EVT_SEQ_GAP:          event_name = "EVT_SEQ_GAP"; break;
        case EventId::EVT_SEQ_REORDER:      event_name = "EVT_SEQ_REORDER"; break;
        case EventId::EVT_FRAME_MALFORMED:  event_name = "EVT_FRAME_MALFORMED"; break;
        case EventId::EVT_CRC_FAIL:         event_name = "EVT_CRC_FAIL"; break;
        case EventId::EVT_SOURCE_ONLINE:    event_name = "EVT_SOURCE_ONLINE"; break;
        case EventId::EVT_SOURCE_TIMEOUT:   event_name = "EVT_SOURCE_TIMEOUT"; break;
        case EventId::EVT_HEARTBEAT_OK:     event_name = "EVT_HEARTBEAT_OK"; break;
        case EventId::EVT_HEARTBEAT_DEGRADE:event_name = "EVT_HEARTBEAT_DEGRADE"; break;
        case EventId::EVT_HEARTBEAT_ERROR:  event_name = "EVT_HEARTBEAT_ERROR"; break;
        case EventId::EVT_CONFIG_CHANGE:    event_name = "EVT_CONFIG_CHANGE"; break;
        case EventId::EVT_WEAPON_STATUS:    event_name = "EVT_WEAPON_STATUS"; break;
        default: break;
    }

    Logger::instance().log(sev, cat, event_name, detail);

    // Publish to event bus
    EventRecord record;
    record.id = id;
    record.category = cat;
    record.severity = sev;
    record.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    record.detail = detail;
    events_.publish(record);
}

} // namespace nng
