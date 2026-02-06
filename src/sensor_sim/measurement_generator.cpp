#include "sensor_sim/measurement_generator.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace nng {

MeasurementGenerator::MeasurementGenerator(uint16_t src_id, uint32_t seed)
    : src_id_(src_id), rng_(seed) {
    std::memset(track_update_counts_, 0, sizeof(track_update_counts_));
}

std::vector<uint8_t> MeasurementGenerator::build_frame(
        MsgType type, const uint8_t* payload,
        uint16_t payload_len, uint64_t timestamp_ns) {
    TelemetryHeader hdr{};
    hdr.version = PROTOCOL_VERSION;
    hdr.msg_type = static_cast<uint8_t>(type);
    hdr.src_id = src_id_;
    hdr.seq = seq_++;
    hdr.ts_ns = timestamp_ns;
    hdr.payload_len = payload_len;

    std::vector<uint8_t> buf(FRAME_HEADER_SIZE + payload_len);
    serialize_header(hdr, buf.data());
    if (payload && payload_len > 0)
        std::memcpy(buf.data() + FRAME_HEADER_SIZE, payload, payload_len);

    return buf;
}

std::vector<std::vector<uint8_t>> MeasurementGenerator::generate_plots(
        const std::vector<WorldObject>& objects, uint64_t timestamp_ns) {
    std::vector<std::vector<uint8_t>> frames;
    frames.reserve(objects.size());

    for (const auto& obj : objects) {
        // Detection probability: p = clamp(rcs_linear / (range_km^2), 0.1, 1.0)
        double rcs_linear = std::pow(10.0, obj.rcs_dbsm / 10.0);
        double range_km = obj.range_m / 1000.0;
        double p_detect = std::clamp(rcs_linear / (range_km * range_km), 0.1, 1.0);

        std::uniform_real_distribution<double> det_dist(0.0, 1.0);
        if (det_dist(rng_) > p_detect)
            continue;

        // Add measurement noise
        std::normal_distribution<double> noise(0.0, obj.noise_stddev);

        PlotPayload pp{};
        pp.plot_id = plot_id_++;
        pp.azimuth_mdeg = static_cast<int32_t>((obj.azimuth_deg + noise(rng_) * 0.01) * 1000.0);
        pp.elevation_mdeg = static_cast<int32_t>((obj.elevation_deg + noise(rng_) * 0.01) * 1000.0);
        pp.range_m = static_cast<uint32_t>(std::max(0.0, obj.range_m + noise(rng_)));
        pp.amplitude_db = static_cast<int16_t>(obj.rcs_dbsm * 10.0 + noise(rng_) * 5.0);
        pp.doppler_mps = static_cast<int16_t>(-obj.speed_mps * std::cos(obj.heading_deg * 3.14159265 / 180.0));
        pp.quality = static_cast<uint8_t>(std::clamp(static_cast<int>(p_detect * 100.0), 10, 100));

        frames.push_back(build_frame(MsgType::PLOT,
            reinterpret_cast<const uint8_t*>(&pp), sizeof(PlotPayload), timestamp_ns));
    }
    return frames;
}

std::vector<std::vector<uint8_t>> MeasurementGenerator::generate_tracks(
        const std::vector<WorldObject>& objects, uint64_t timestamp_ns) {
    std::vector<std::vector<uint8_t>> frames;
    frames.reserve(objects.size());

    std::normal_distribution<double> noise(0.0, 1.0);

    for (const auto& obj : objects) {
        uint16_t tid_idx = static_cast<uint16_t>(obj.id & 0xFFFF);

        TrackPayload tp{};
        tp.track_id = obj.id;
        tp.classification = static_cast<uint8_t>(obj.classification);

        // Threat level based on hostility and classification
        if (!obj.is_hostile) {
            tp.threat_level = static_cast<uint8_t>(ThreatLevel::LOW);
        } else {
            switch (obj.classification) {
                case TrackClass::MISSILE:
                case TrackClass::ROCKET_ARTILLERY:
                    tp.threat_level = static_cast<uint8_t>(ThreatLevel::CRITICAL);
                    break;
                case TrackClass::UAV_SMALL:
                case TrackClass::UAV_LARGE:
                    tp.threat_level = static_cast<uint8_t>(ThreatLevel::HIGH);
                    break;
                default:
                    tp.threat_level = static_cast<uint8_t>(ThreatLevel::MEDIUM);
                    break;
            }
        }

        tp.iff_status = obj.is_hostile
            ? static_cast<uint8_t>(IffStatus::FOE)
            : static_cast<uint8_t>(IffStatus::FRIEND);

        tp.azimuth_mdeg = static_cast<int32_t>(obj.azimuth_deg * 1000.0 + noise(rng_) * obj.noise_stddev * 10.0);
        tp.elevation_mdeg = static_cast<int32_t>(obj.elevation_deg * 1000.0 + noise(rng_) * obj.noise_stddev * 10.0);
        tp.range_m = static_cast<uint32_t>(std::max(0.0, obj.range_m + noise(rng_) * obj.noise_stddev));
        tp.velocity_mps = static_cast<int16_t>(-obj.speed_mps * std::cos(obj.heading_deg * 3.14159265 / 180.0));
        tp.rcs_dbsm = static_cast<int16_t>(obj.rcs_dbsm * 100.0);
        tp.update_count = ++track_update_counts_[tid_idx];

        frames.push_back(build_frame(MsgType::TRACK,
            reinterpret_cast<const uint8_t*>(&tp), sizeof(TrackPayload), timestamp_ns));
    }
    return frames;
}

std::vector<uint8_t> MeasurementGenerator::generate_heartbeat(uint64_t timestamp_ns) {
    std::uniform_int_distribution<int> cpu_dist(10, 60);
    std::uniform_int_distribution<int> mem_dist(20, 70);

    HeartbeatPayload hb{};
    hb.subsystem_id = src_id_;
    hb.state = static_cast<uint8_t>(SubsystemState::OK);
    hb.cpu_pct = static_cast<uint8_t>(cpu_dist(rng_));
    hb.mem_pct = static_cast<uint8_t>(mem_dist(rng_));
    hb.uptime_s = static_cast<uint32_t>(timestamp_ns / 1000000000ULL);
    hb.error_code = 0;

    return build_frame(MsgType::HEARTBEAT,
        reinterpret_cast<const uint8_t*>(&hb), sizeof(HeartbeatPayload), timestamp_ns);
}

std::vector<uint8_t> MeasurementGenerator::generate_engagement(
        uint16_t weapon_id, WeaponMode mode, uint32_t assigned_track,
        uint16_t rounds, int16_t barrel_temp, uint16_t bursts,
        uint64_t timestamp_ns) {
    EngagementPayload ep{};
    ep.weapon_id = weapon_id;
    ep.mode = static_cast<uint8_t>(mode);
    ep.assigned_track = assigned_track;
    ep.rounds_remaining = rounds;
    ep.barrel_temp_c = barrel_temp;
    ep.burst_count = bursts;

    return build_frame(MsgType::ENGAGEMENT,
        reinterpret_cast<const uint8_t*>(&ep), sizeof(EngagementPayload), timestamp_ns);
}

} // namespace nng
