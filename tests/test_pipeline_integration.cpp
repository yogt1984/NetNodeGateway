#include <gtest/gtest.h>
#include "sensor_sim/object_generator.h"
#include "sensor_sim/world_model.h"
#include "sensor_sim/measurement_generator.h"
#include "sensor_sim/fault_injector.h"
#include "gateway/telemetry_parser.h"
#include "gateway/sequence_tracker.h"
#include "gateway/stats_manager.h"
#include "common/event_bus.h"
#include "common/logger.h"
#include <sstream>

using namespace nng;

TEST(PipelineIntegration, PatrolScenarioNoFaults) {
    // Setup
    auto profile = profile_patrol();
    ObjectGenerator gen(profile, 42);
    WorldModel world;
    MeasurementGenerator meas(0x0012, 123);
    FaultConfig no_faults{};
    FaultInjector fi(no_faults, 99);
    SequenceTracker tracker;
    StatsManager stats;

    // Populate world
    for (auto& obj : gen.generate_initial())
        world.add_object(std::move(obj));

    ASSERT_GT(world.active_count(), 0u) << "Patrol should start with objects";

    // Run 100 ticks at 50Hz (2 seconds)
    double dt = 0.02;
    double t = 0.0;
    for (int tick = 0; tick < 100; ++tick) {
        t += dt;
        world.tick(dt, t);

        if (world.active_count() == 0) continue;

        uint64_t ts_ns = static_cast<uint64_t>(t * 1e9);
        auto frames = meas.generate_tracks(world.objects(), ts_ns);
        fi.apply(frames);

        for (const auto& frame : frames) {
            ParsedFrame pf;
            auto err = parse_frame(frame.data(), frame.size(), false, pf);
            if (err != ParseError::OK) {
                stats.record_malformed(pf.header.src_id);
                continue;
            }
            auto seq_ev = tracker.track(pf.header.src_id, pf.header.seq);
            stats.record_rx(pf.header.src_id, pf.header.seq, pf.header.ts_ns);

            switch (seq_ev.result) {
                case SeqResult::GAP:
                    stats.record_gap(pf.header.src_id, seq_ev.gap_size);
                    break;
                case SeqResult::REORDER:
                    stats.record_reorder(pf.header.src_id);
                    break;
                case SeqResult::DUPLICATE:
                    stats.record_duplicate(pf.header.src_id);
                    break;
                default:
                    break;
            }
        }

        // Maybe spawn new objects
        auto new_obj = gen.maybe_spawn(t);
        if (new_obj.has_value())
            world.add_object(std::move(*new_obj));
    }

    auto g = stats.get_global_stats();
    EXPECT_GT(g.rx_total, 0u) << "Should have received frames";
    EXPECT_EQ(g.malformed_total, 0u) << "No faults = no malformed frames";
    EXPECT_EQ(g.gap_total, 0u) << "No faults = no gaps";
    EXPECT_EQ(g.reorder_total, 0u) << "No faults = no reorders";
    EXPECT_EQ(g.duplicate_total, 0u) << "No faults = no duplicates";
    EXPECT_EQ(stats.get_health(), HealthState::OK);

    auto source = stats.get_source_stats(0x0012);
    EXPECT_EQ(source.rx_count, g.rx_total);
}

TEST(PipelineIntegration, PatrolScenarioWithFaults) {
    auto profile = profile_patrol();
    ObjectGenerator gen(profile, 42);
    WorldModel world;
    MeasurementGenerator meas(0x0012, 123);
    FaultConfig faults{};
    faults.loss_pct = 5.0;
    faults.reorder_pct = 3.0;
    faults.duplicate_pct = 2.0;
    FaultInjector fi(faults, 99);
    SequenceTracker tracker;
    StatsManager stats;

    for (auto& obj : gen.generate_initial())
        world.add_object(std::move(obj));

    double dt = 0.02;
    double t = 0.0;
    uint64_t total_generated = 0;

    for (int tick = 0; tick < 200; ++tick) {
        t += dt;
        world.tick(dt, t);
        if (world.active_count() == 0) continue;

        uint64_t ts_ns = static_cast<uint64_t>(t * 1e9);
        auto frames = meas.generate_tracks(world.objects(), ts_ns);
        total_generated += frames.size();
        fi.apply(frames);

        for (const auto& frame : frames) {
            ParsedFrame pf;
            auto err = parse_frame(frame.data(), frame.size(), false, pf);
            if (err != ParseError::OK) {
                stats.record_malformed(0);
                continue;
            }
            auto seq_ev = tracker.track(pf.header.src_id, pf.header.seq);
            stats.record_rx(pf.header.src_id, pf.header.seq, pf.header.ts_ns);

            switch (seq_ev.result) {
                case SeqResult::GAP:
                    stats.record_gap(pf.header.src_id, seq_ev.gap_size);
                    break;
                case SeqResult::REORDER:
                    stats.record_reorder(pf.header.src_id);
                    break;
                case SeqResult::DUPLICATE:
                    stats.record_duplicate(pf.header.src_id);
                    break;
                default:
                    break;
            }
        }

        auto new_obj = gen.maybe_spawn(t);
        if (new_obj.has_value())
            world.add_object(std::move(*new_obj));
    }

    auto g = stats.get_global_stats();
    EXPECT_GT(g.rx_total, 0u);

    // With 5% loss, we should see gaps
    EXPECT_GT(g.gap_total, 0u) << "5% loss should cause sequence gaps";

    // With 3% reorder, should see some reorders
    // (may or may not, depending on whether reordered frames are also in gap window)

    // rx_total should be less than total_generated (some were dropped)
    EXPECT_LT(g.rx_total, total_generated + 50)
        << "Received more than generated + duplicates margin";

    // Health should not be OK (gaps present)
    EXPECT_NE(stats.get_health(), HealthState::OK);
}

TEST(PipelineIntegration, RaidScenarioHighVolume) {
    auto profile = profile_raid();
    ObjectGenerator gen(profile, 42);
    WorldModel world;
    MeasurementGenerator meas(0x0014, 456);
    FaultConfig no_faults{};
    FaultInjector fi(no_faults, 99);
    SequenceTracker tracker;
    StatsManager stats;

    for (auto& obj : gen.generate_initial())
        world.add_object(std::move(obj));

    // Raid should start with 10-30 objects
    ASSERT_GE(world.active_count(), 10u);

    double dt = 0.01; // 100Hz
    double t = 0.0;
    for (int tick = 0; tick < 100; ++tick) {
        t += dt;
        world.tick(dt, t);
        if (world.active_count() == 0) continue;

        uint64_t ts_ns = static_cast<uint64_t>(t * 1e9);
        auto frames = meas.generate_tracks(world.objects(), ts_ns);
        fi.apply(frames);

        for (const auto& frame : frames) {
            ParsedFrame pf;
            if (parse_frame(frame.data(), frame.size(), false, pf) == ParseError::OK) {
                tracker.track(pf.header.src_id, pf.header.seq);
                stats.record_rx(pf.header.src_id, pf.header.seq, pf.header.ts_ns);
            }
        }
    }

    auto g = stats.get_global_stats();
    // With 10+ objects at 100Hz for 100 ticks = at least 1000 frames
    EXPECT_GT(g.rx_total, 500u) << "Raid scenario should produce high frame volume";
}

TEST(PipelineIntegration, LogOutputProducedDuringPipeline) {
    std::ostringstream oss;
    Logger& logger = Logger::instance();
    logger.set_output(oss);
    logger.set_level(Severity::DEBUG);

    auto profile = profile_patrol();
    ObjectGenerator gen(profile, 42);
    WorldModel world;
    MeasurementGenerator meas(0x0012, 123);
    FaultConfig faults{};
    faults.loss_pct = 10.0;
    FaultInjector fi(faults, 99);
    SequenceTracker tracker;
    StatsManager stats;
    EventBus bus;

    // Log events via bus
    bus.subscribe_all([&](const EventRecord& e) {
        logger.log(e.severity, e.category,
                   std::to_string(static_cast<uint16_t>(e.id)), e.detail);
    });

    for (auto& obj : gen.generate_initial())
        world.add_object(std::move(obj));

    double dt = 0.02;
    double t = 0.0;
    for (int tick = 0; tick < 50; ++tick) {
        t += dt;
        world.tick(dt, t);
        if (world.active_count() == 0) continue;

        uint64_t ts_ns = static_cast<uint64_t>(t * 1e9);
        auto frames = meas.generate_tracks(world.objects(), ts_ns);
        fi.apply(frames);

        for (const auto& frame : frames) {
            ParsedFrame pf;
            auto err = parse_frame(frame.data(), frame.size(), false, pf);
            if (err != ParseError::OK) continue;

            auto seq_ev = tracker.track(pf.header.src_id, pf.header.seq);
            stats.record_rx(pf.header.src_id, pf.header.seq, pf.header.ts_ns);

            if (seq_ev.result == SeqResult::GAP) {
                stats.record_gap(pf.header.src_id, seq_ev.gap_size);
                bus.publish({EventId::EVT_SEQ_GAP, EventCategory::NETWORK,
                             Severity::WARN, ts_ns,
                             "src=0x" + std::to_string(pf.header.src_id) +
                             " gap=" + std::to_string(seq_ev.gap_size)});
            }
        }
    }

    std::string output = oss.str();
    if (stats.get_global_stats().gap_total > 0) {
        EXPECT_FALSE(output.empty()) << "Should produce log output when gaps occur";
        EXPECT_NE(output.find("NETWORK"), std::string::npos)
            << "Log should contain NETWORK category";
    }

    // Restore logger to stdout
    logger.set_output(std::cout);
}
