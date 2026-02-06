#include <gtest/gtest.h>
#include "gateway/telemetry_parser.h"
#include "common/protocol.h"
#include "common/crc32.h"
#include <vector>
#include <cstring>

using namespace nng;

// Helper: build a raw frame buffer from header + payload, optionally append CRC
static std::vector<uint8_t> build_frame(const TelemetryHeader& hdr,
                                         const uint8_t* payload,
                                         bool append_crc) {
    std::vector<uint8_t> buf(FRAME_HEADER_SIZE + hdr.payload_len);
    serialize_header(hdr, buf.data());
    if (payload && hdr.payload_len > 0)
        std::memcpy(buf.data() + FRAME_HEADER_SIZE, payload, hdr.payload_len);

    if (append_crc) {
        uint32_t crc = crc32(buf.data(), buf.size());
        buf.resize(buf.size() + FRAME_CRC_SIZE);
        std::memcpy(buf.data() + buf.size() - FRAME_CRC_SIZE, &crc, sizeof(crc));
    }
    return buf;
}

TEST(TelemetryParser, ValidTrackNoCrc) {
    TrackPayload tp{};
    tp.track_id = 42;
    tp.classification = static_cast<uint8_t>(TrackClass::MISSILE);
    tp.threat_level = static_cast<uint8_t>(ThreatLevel::HIGH);
    tp.azimuth_mdeg = 180000;
    tp.range_m = 5000;
    tp.velocity_mps = -200;

    TelemetryHeader hdr{};
    hdr.version = PROTOCOL_VERSION;
    hdr.msg_type = static_cast<uint8_t>(MsgType::TRACK);
    hdr.src_id = 0x0012;
    hdr.seq = 1;
    hdr.ts_ns = 1000000;
    hdr.payload_len = sizeof(TrackPayload);

    auto buf = build_frame(hdr, reinterpret_cast<const uint8_t*>(&tp), false);

    ParsedFrame pf;
    EXPECT_EQ(parse_frame(buf.data(), buf.size(), false, pf), ParseError::OK);
    EXPECT_EQ(pf.header.src_id, 0x0012);
    EXPECT_EQ(pf.header.seq, 1u);
    EXPECT_EQ(pf.header.msg_type, static_cast<uint8_t>(MsgType::TRACK));

    auto parsed_tp = deserialize_track(pf.payload_ptr);
    EXPECT_EQ(parsed_tp.track_id, 42u);
    EXPECT_EQ(parsed_tp.velocity_mps, -200);
}

TEST(TelemetryParser, ValidPlotWithCrc) {
    PlotPayload pp{};
    pp.plot_id = 7;
    pp.azimuth_mdeg = 90000;
    pp.range_m = 12000;
    pp.quality = 85;

    TelemetryHeader hdr{};
    hdr.version = PROTOCOL_VERSION;
    hdr.msg_type = static_cast<uint8_t>(MsgType::PLOT);
    hdr.src_id = 0x0001;
    hdr.seq = 0;
    hdr.ts_ns = 500000;
    hdr.payload_len = sizeof(PlotPayload);

    auto buf = build_frame(hdr, reinterpret_cast<const uint8_t*>(&pp), true);

    ParsedFrame pf;
    EXPECT_EQ(parse_frame(buf.data(), buf.size(), true, pf), ParseError::OK);
    EXPECT_TRUE(pf.has_crc);

    auto parsed_pp = deserialize_plot(pf.payload_ptr);
    EXPECT_EQ(parsed_pp.plot_id, 7u);
    EXPECT_EQ(parsed_pp.quality, 85);
}

TEST(TelemetryParser, TooShort) {
    uint8_t buf[10] = {};
    ParsedFrame pf;
    EXPECT_EQ(parse_frame(buf, 10, false, pf), ParseError::TOO_SHORT);
}

TEST(TelemetryParser, BadVersion) {
    TelemetryHeader hdr{};
    hdr.version = 99;
    hdr.msg_type = static_cast<uint8_t>(MsgType::TRACK);
    hdr.payload_len = 0;
    auto buf = build_frame(hdr, nullptr, false);

    ParsedFrame pf;
    EXPECT_EQ(parse_frame(buf.data(), buf.size(), false, pf), ParseError::BAD_VERSION);
}

TEST(TelemetryParser, BadMsgType) {
    TelemetryHeader hdr{};
    hdr.version = PROTOCOL_VERSION;
    hdr.msg_type = 0xFF;
    hdr.payload_len = 0;
    auto buf = build_frame(hdr, nullptr, false);

    ParsedFrame pf;
    EXPECT_EQ(parse_frame(buf.data(), buf.size(), false, pf), ParseError::BAD_MSG_TYPE);
}

TEST(TelemetryParser, PayloadTooLong) {
    TelemetryHeader hdr{};
    hdr.version = PROTOCOL_VERSION;
    hdr.msg_type = static_cast<uint8_t>(MsgType::TRACK);
    hdr.payload_len = 2000; // > MAX_PAYLOAD_SIZE

    std::vector<uint8_t> buf(FRAME_HEADER_SIZE);
    serialize_header(hdr, buf.data());

    ParsedFrame pf;
    EXPECT_EQ(parse_frame(buf.data(), buf.size(), false, pf), ParseError::PAYLOAD_TOO_LONG);
}

TEST(TelemetryParser, Truncated) {
    TelemetryHeader hdr{};
    hdr.version = PROTOCOL_VERSION;
    hdr.msg_type = static_cast<uint8_t>(MsgType::TRACK);
    hdr.payload_len = 100;

    // Only provide header + 50 bytes of payload (100 expected)
    std::vector<uint8_t> buf(FRAME_HEADER_SIZE + 50, 0);
    serialize_header(hdr, buf.data());

    ParsedFrame pf;
    EXPECT_EQ(parse_frame(buf.data(), buf.size(), false, pf), ParseError::TRUNCATED);
}

TEST(TelemetryParser, CrcMismatch) {
    TrackPayload tp{};
    tp.track_id = 1;

    TelemetryHeader hdr{};
    hdr.version = PROTOCOL_VERSION;
    hdr.msg_type = static_cast<uint8_t>(MsgType::TRACK);
    hdr.src_id = 1;
    hdr.seq = 0;
    hdr.payload_len = sizeof(TrackPayload);

    auto buf = build_frame(hdr, reinterpret_cast<const uint8_t*>(&tp), true);
    // Corrupt the CRC bytes
    buf[buf.size() - 1] ^= 0xFF;

    ParsedFrame pf;
    EXPECT_EQ(parse_frame(buf.data(), buf.size(), true, pf), ParseError::CRC_MISMATCH);
}

TEST(TelemetryParser, ValidHeartbeat) {
    HeartbeatPayload hb{};
    hb.subsystem_id = 3;
    hb.state = static_cast<uint8_t>(SubsystemState::OK);
    hb.cpu_pct = 45;
    hb.mem_pct = 62;
    hb.uptime_s = 86400;
    hb.error_code = 0;

    TelemetryHeader hdr{};
    hdr.version = PROTOCOL_VERSION;
    hdr.msg_type = static_cast<uint8_t>(MsgType::HEARTBEAT);
    hdr.src_id = 0x0005;
    hdr.seq = 100;
    hdr.ts_ns = 999999;
    hdr.payload_len = sizeof(HeartbeatPayload);

    auto buf = build_frame(hdr, reinterpret_cast<const uint8_t*>(&hb), false);

    ParsedFrame pf;
    EXPECT_EQ(parse_frame(buf.data(), buf.size(), false, pf), ParseError::OK);
    auto parsed = deserialize_heartbeat(pf.payload_ptr);
    EXPECT_EQ(parsed.subsystem_id, 3);
    EXPECT_EQ(parsed.cpu_pct, 45);
    EXPECT_EQ(parsed.uptime_s, 86400u);
}

TEST(TelemetryParser, ValidEngagement) {
    EngagementPayload ep{};
    ep.weapon_id = 2;
    ep.mode = static_cast<uint8_t>(WeaponMode::ENGAGING);
    ep.assigned_track = 1042;
    ep.rounds_remaining = 480;
    ep.barrel_temp_c = 87;
    ep.burst_count = 4;

    TelemetryHeader hdr{};
    hdr.version = PROTOCOL_VERSION;
    hdr.msg_type = static_cast<uint8_t>(MsgType::ENGAGEMENT);
    hdr.src_id = 0x0020;
    hdr.seq = 55;
    hdr.ts_ns = 123456789;
    hdr.payload_len = sizeof(EngagementPayload);

    auto buf = build_frame(hdr, reinterpret_cast<const uint8_t*>(&ep), false);

    ParsedFrame pf;
    EXPECT_EQ(parse_frame(buf.data(), buf.size(), false, pf), ParseError::OK);
    auto parsed = deserialize_engagement(pf.payload_ptr);
    EXPECT_EQ(parsed.weapon_id, 2);
    EXPECT_EQ(parsed.assigned_track, 1042u);
    EXPECT_EQ(parsed.rounds_remaining, 480);
}
