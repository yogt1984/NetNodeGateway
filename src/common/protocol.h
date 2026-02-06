#pragma once
#include "common/types.h"
#include <cstring>
#include <cstdint>

namespace nng {

#pragma pack(push, 1)

struct TelemetryHeader {
    uint8_t  version;
    uint8_t  msg_type;
    uint16_t src_id;
    uint32_t seq;
    uint64_t ts_ns;
    uint16_t payload_len;
};
static_assert(sizeof(TelemetryHeader) == 18, "TelemetryHeader must be 18 bytes");

struct PlotPayload {
    uint32_t plot_id;
    int32_t  azimuth_mdeg;
    int32_t  elevation_mdeg;
    uint32_t range_m;
    int16_t  amplitude_db;
    int16_t  doppler_mps;
    uint8_t  quality;
};
static_assert(sizeof(PlotPayload) == 21, "PlotPayload must be 21 bytes");

struct TrackPayload {
    uint32_t track_id;
    uint8_t  classification;
    uint8_t  threat_level;
    uint8_t  iff_status;
    int32_t  azimuth_mdeg;
    int32_t  elevation_mdeg;
    uint32_t range_m;
    int16_t  velocity_mps;
    int16_t  rcs_dbsm;
    uint16_t update_count;
};
static_assert(sizeof(TrackPayload) == 25, "TrackPayload must be 25 bytes");

struct HeartbeatPayload {
    uint16_t subsystem_id;
    uint8_t  state;
    uint8_t  cpu_pct;
    uint8_t  mem_pct;
    uint32_t uptime_s;
    uint16_t error_code;
};
static_assert(sizeof(HeartbeatPayload) == 11, "HeartbeatPayload must be 11 bytes");

struct EngagementPayload {
    uint16_t weapon_id;
    uint8_t  mode;
    uint32_t assigned_track;
    uint16_t rounds_remaining;
    int16_t  barrel_temp_c;
    uint16_t burst_count;
};
static_assert(sizeof(EngagementPayload) == 13, "EngagementPayload must be 13 bytes");

#pragma pack(pop)

// --- Serialization (little-endian, memcpy-based for packed structs on x86) ---

inline void serialize_header(const TelemetryHeader& h, uint8_t* buf) {
    std::memcpy(buf, &h, sizeof(TelemetryHeader));
}

inline TelemetryHeader deserialize_header(const uint8_t* buf) {
    TelemetryHeader h;
    std::memcpy(&h, buf, sizeof(TelemetryHeader));
    return h;
}

inline void serialize_plot(const PlotPayload& p, uint8_t* buf) {
    std::memcpy(buf, &p, sizeof(PlotPayload));
}

inline PlotPayload deserialize_plot(const uint8_t* buf) {
    PlotPayload p;
    std::memcpy(&p, buf, sizeof(PlotPayload));
    return p;
}

inline void serialize_track(const TrackPayload& t, uint8_t* buf) {
    std::memcpy(buf, &t, sizeof(TrackPayload));
}

inline TrackPayload deserialize_track(const uint8_t* buf) {
    TrackPayload t;
    std::memcpy(&t, buf, sizeof(TrackPayload));
    return t;
}

inline void serialize_heartbeat(const HeartbeatPayload& hb, uint8_t* buf) {
    std::memcpy(buf, &hb, sizeof(HeartbeatPayload));
}

inline HeartbeatPayload deserialize_heartbeat(const uint8_t* buf) {
    HeartbeatPayload hb;
    std::memcpy(&hb, buf, sizeof(HeartbeatPayload));
    return hb;
}

inline void serialize_engagement(const EngagementPayload& e, uint8_t* buf) {
    std::memcpy(buf, &e, sizeof(EngagementPayload));
}

inline EngagementPayload deserialize_engagement(const uint8_t* buf) {
    EngagementPayload e;
    std::memcpy(&e, buf, sizeof(EngagementPayload));
    return e;
}

} // namespace nng
