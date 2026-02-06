#pragma once
#include "common/protocol.h"
#include "common/types.h"
#include "sensor_sim/object_generator.h"
#include <vector>
#include <random>
#include <cstdint>

namespace nng {

class MeasurementGenerator {
public:
    explicit MeasurementGenerator(uint16_t src_id, uint32_t seed = 123);

    // Generate PLOT frames from world objects (raw detections with noise).
    // Objects may not be detected based on RCS/range probability.
    std::vector<std::vector<uint8_t>> generate_plots(
        const std::vector<WorldObject>& objects, uint64_t timestamp_ns);

    // Generate TRACK frames from world objects (associated detections).
    std::vector<std::vector<uint8_t>> generate_tracks(
        const std::vector<WorldObject>& objects, uint64_t timestamp_ns);

    // Generate a HEARTBEAT frame.
    std::vector<uint8_t> generate_heartbeat(uint64_t timestamp_ns);

    // Generate an ENGAGEMENT_STATUS frame.
    std::vector<uint8_t> generate_engagement(
        uint16_t weapon_id, WeaponMode mode, uint32_t assigned_track,
        uint16_t rounds, int16_t barrel_temp, uint16_t bursts,
        uint64_t timestamp_ns);

    uint32_t seq() const { return seq_; }

private:
    std::vector<uint8_t> build_frame(MsgType type, const uint8_t* payload,
                                      uint16_t payload_len, uint64_t timestamp_ns);

    uint16_t src_id_;
    uint32_t seq_ = 0;
    std::mt19937 rng_;
    uint32_t plot_id_ = 1;
    uint16_t track_update_counts_[65536] = {}; // per track_id update counter
};

} // namespace nng
