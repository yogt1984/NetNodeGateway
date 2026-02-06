#pragma once
#include "gateway/frame_source.h"
#include "gateway/telemetry_parser.h"
#include "gateway/sequence_tracker.h"
#include "gateway/stats_manager.h"
#include "gateway/frame_recorder.h"
#include "common/logger.h"
#include "common/event_bus.h"
#include "common/types.h"
#include <string>
#include <atomic>
#include <memory>

namespace nng {

struct GatewayConfig {
    uint16_t udp_port        = 5000;
    bool     crc_enabled     = true;
    bool     record_enabled  = false;
    std::string record_path  = "./recorded/session.bin";
    std::string replay_path;  // if non-empty, use replay instead of UDP
    Severity log_level       = Severity::INFO;
};

class Gateway {
public:
    explicit Gateway(const GatewayConfig& config);
    ~Gateway();

    // Run the main loop (blocking, until stop() is called from another thread)
    void run();
    void stop();

    bool is_running() const { return running_.load(); }

    // Accessors for control node integration
    StatsManager& stats() { return stats_; }
    EventBus& events() { return events_; }
    Logger& logger() { return Logger::instance(); }

    // Get config
    const GatewayConfig& config() const { return config_; }

private:
    void process_frame(const std::vector<uint8_t>& frame, uint64_t rx_timestamp_ns);
    void publish_event(EventId id, EventCategory cat, Severity sev, const std::string& detail);

    GatewayConfig config_;
    std::unique_ptr<IFrameSource> source_;
    SequenceTracker tracker_;
    StatsManager stats_;
    EventBus events_;
    FrameRecorder recorder_;

    std::atomic<bool> running_{false};
    std::atomic<bool> should_stop_{false};
};

} // namespace nng
