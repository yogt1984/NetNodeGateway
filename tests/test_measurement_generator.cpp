#include <gtest/gtest.h>
#include "sensor_sim/measurement_generator.h"
#include "gateway/telemetry_parser.h"
#include "common/protocol.h"
#include <cmath>

using namespace nng;

static WorldObject make_close_object() {
    WorldObject obj{};
    obj.id = 1;
    obj.classification = TrackClass::FIXED_WING;
    obj.spawn_time_s = 0.0;
    obj.lifetime_s = 60.0;
    obj.azimuth_deg = 45.0;
    obj.elevation_deg = 10.0;
    obj.range_m = 5000.0;
    obj.speed_mps = 200.0;
    obj.heading_deg = 180.0;
    obj.rcs_dbsm = 10.0; // large RCS = high detection probability
    obj.is_hostile = true;
    obj.noise_stddev = 1.0;
    return obj;
}

static WorldObject make_far_stealth_object() {
    WorldObject obj{};
    obj.id = 2;
    obj.classification = TrackClass::UAV_SMALL;
    obj.spawn_time_s = 0.0;
    obj.lifetime_s = 60.0;
    obj.azimuth_deg = 200.0;
    obj.elevation_deg = 5.0;
    obj.range_m = 40000.0;
    obj.speed_mps = 50.0;
    obj.heading_deg = 0.0;
    obj.rcs_dbsm = -20.0; // very small RCS
    obj.is_hostile = false;
    obj.noise_stddev = 50.0;
    return obj;
}

TEST(MeasurementGenerator, GenerateTracksOneObject) {
    MeasurementGenerator mg(0x0012, 42);
    std::vector<WorldObject> objects = {make_close_object()};
    auto frames = mg.generate_tracks(objects, 1000000);
    ASSERT_EQ(frames.size(), 1u);
}

TEST(MeasurementGenerator, TrackFrameParsesCleanly) {
    MeasurementGenerator mg(0x0012, 42);
    std::vector<WorldObject> objects = {make_close_object()};
    auto frames = mg.generate_tracks(objects, 1000000);
    ASSERT_EQ(frames.size(), 1u);

    ParsedFrame pf;
    auto err = parse_frame(frames[0].data(), frames[0].size(), false, pf);
    EXPECT_EQ(err, ParseError::OK);
    EXPECT_EQ(pf.header.msg_type, static_cast<uint8_t>(MsgType::TRACK));
    EXPECT_EQ(pf.header.src_id, 0x0012);
}

TEST(MeasurementGenerator, TrackPayloadFieldsReasonable) {
    MeasurementGenerator mg(0x0001, 42);
    auto obj = make_close_object();
    auto frames = mg.generate_tracks({obj}, 5000000);
    ASSERT_EQ(frames.size(), 1u);

    ParsedFrame pf;
    ASSERT_EQ(parse_frame(frames[0].data(), frames[0].size(), false, pf), ParseError::OK);
    auto tp = deserialize_track(pf.payload_ptr);

    EXPECT_EQ(tp.track_id, obj.id);
    EXPECT_EQ(tp.classification, static_cast<uint8_t>(TrackClass::FIXED_WING));
    // Hostile missile-class gets CRITICAL, but FIXED_WING hostile gets MEDIUM
    EXPECT_EQ(tp.threat_level, static_cast<uint8_t>(ThreatLevel::MEDIUM));
    EXPECT_EQ(tp.iff_status, static_cast<uint8_t>(IffStatus::FOE));
    // Azimuth should be near 45000 mdeg (45.0 * 1000) +/- noise
    EXPECT_NEAR(tp.azimuth_mdeg, 45000, 500);
    EXPECT_GT(tp.range_m, 0u);
    EXPECT_EQ(tp.update_count, 1);
}

TEST(MeasurementGenerator, SequenceNumbersIncrement) {
    MeasurementGenerator mg(0x0001, 42);
    auto obj = make_close_object();

    auto frames1 = mg.generate_tracks({obj}, 1000000);
    auto frames2 = mg.generate_tracks({obj}, 2000000);
    auto frames3 = mg.generate_heartbeat(3000000);

    ParsedFrame pf1, pf2, pf3;
    parse_frame(frames1[0].data(), frames1[0].size(), false, pf1);
    parse_frame(frames2[0].data(), frames2[0].size(), false, pf2);
    parse_frame(frames3.data(), frames3.size(), false, pf3);

    EXPECT_EQ(pf1.header.seq, 0u);
    EXPECT_EQ(pf2.header.seq, 1u);
    EXPECT_EQ(pf3.header.seq, 2u);
}

TEST(MeasurementGenerator, HeartbeatFrameParsesCleanly) {
    MeasurementGenerator mg(0x0005, 42);
    auto frame = mg.generate_heartbeat(99999999);

    ParsedFrame pf;
    auto err = parse_frame(frame.data(), frame.size(), false, pf);
    EXPECT_EQ(err, ParseError::OK);
    EXPECT_EQ(pf.header.msg_type, static_cast<uint8_t>(MsgType::HEARTBEAT));

    auto hb = deserialize_heartbeat(pf.payload_ptr);
    EXPECT_EQ(hb.subsystem_id, 0x0005);
    EXPECT_EQ(hb.state, static_cast<uint8_t>(SubsystemState::OK));
    EXPECT_GE(hb.cpu_pct, 10);
    EXPECT_LE(hb.cpu_pct, 60);
    EXPECT_GE(hb.mem_pct, 20);
    EXPECT_LE(hb.mem_pct, 70);
}

TEST(MeasurementGenerator, EngagementFrameParsesCleanly) {
    MeasurementGenerator mg(0x0020, 42);
    auto frame = mg.generate_engagement(
        3, WeaponMode::ENGAGING, 1042, 480, 87, 4, 123456789);

    ParsedFrame pf;
    auto err = parse_frame(frame.data(), frame.size(), false, pf);
    EXPECT_EQ(err, ParseError::OK);
    EXPECT_EQ(pf.header.msg_type, static_cast<uint8_t>(MsgType::ENGAGEMENT));

    auto ep = deserialize_engagement(pf.payload_ptr);
    EXPECT_EQ(ep.weapon_id, 3);
    EXPECT_EQ(ep.mode, static_cast<uint8_t>(WeaponMode::ENGAGING));
    EXPECT_EQ(ep.assigned_track, 1042u);
    EXPECT_EQ(ep.rounds_remaining, 480);
    EXPECT_EQ(ep.barrel_temp_c, 87);
    EXPECT_EQ(ep.burst_count, 4);
}

TEST(MeasurementGenerator, PlotsCanMissDetection) {
    // Far away, tiny RCS object -> low detection probability
    MeasurementGenerator mg(0x0001, 42);
    auto obj = make_far_stealth_object();

    int total_detected = 0;
    int total_attempts = 200;
    for (int i = 0; i < total_attempts; ++i) {
        MeasurementGenerator mg_i(0x0001, static_cast<uint32_t>(i));
        auto plots = mg_i.generate_plots({obj}, static_cast<uint64_t>(i) * 1000);
        total_detected += static_cast<int>(plots.size());
    }
    // With very low RCS at 40km, detection rate should be low (p ~= 0.1)
    double rate = static_cast<double>(total_detected) / total_attempts;
    EXPECT_LT(rate, 0.5) << "Far stealth target should have low detection rate, got " << rate;
    EXPECT_GT(rate, 0.01) << "Should still detect sometimes (minimum p=0.1)";
}

TEST(MeasurementGenerator, PlotsFromCloseHighRcsDetectedOften) {
    MeasurementGenerator mg(0x0001, 42);
    auto obj = make_close_object(); // close, high RCS

    int total_detected = 0;
    int total_attempts = 100;
    for (int i = 0; i < total_attempts; ++i) {
        MeasurementGenerator mg_i(0x0001, static_cast<uint32_t>(i));
        auto plots = mg_i.generate_plots({obj}, static_cast<uint64_t>(i) * 1000);
        total_detected += static_cast<int>(plots.size());
    }
    double rate = static_cast<double>(total_detected) / total_attempts;
    // RCS=10dBsm (linear=10), range=5km -> p = 10/25 = 0.4, clamped to [0.1, 1.0]
    EXPECT_GT(rate, 0.25) << "Close high-RCS target detection rate too low, got " << rate;
}

TEST(MeasurementGenerator, PlotFrameParsesCleanly) {
    MeasurementGenerator mg(0x0001, 42);
    auto obj = make_close_object();
    auto frames = mg.generate_plots({obj}, 1000000);
    // May or may not detect (probabilistic), but if detected, must parse
    for (const auto& frame : frames) {
        ParsedFrame pf;
        auto err = parse_frame(frame.data(), frame.size(), false, pf);
        EXPECT_EQ(err, ParseError::OK);
        EXPECT_EQ(pf.header.msg_type, static_cast<uint8_t>(MsgType::PLOT));
    }
}

TEST(MeasurementGenerator, MultipleObjectsMultipleFrames) {
    MeasurementGenerator mg(0x0001, 42);
    std::vector<WorldObject> objects;
    for (int i = 0; i < 5; ++i) {
        auto obj = make_close_object();
        obj.id = static_cast<uint32_t>(i + 1);
        objects.push_back(obj);
    }
    auto frames = mg.generate_tracks(objects, 1000000);
    EXPECT_EQ(frames.size(), 5u);
}

TEST(MeasurementGenerator, UpdateCountIncrements) {
    MeasurementGenerator mg(0x0001, 42);
    auto obj = make_close_object();

    mg.generate_tracks({obj}, 1000000);
    auto frames2 = mg.generate_tracks({obj}, 2000000);

    ParsedFrame pf;
    parse_frame(frames2[0].data(), frames2[0].size(), false, pf);
    auto tp = deserialize_track(pf.payload_ptr);
    EXPECT_EQ(tp.update_count, 2) << "Second track update should have update_count=2";
}
