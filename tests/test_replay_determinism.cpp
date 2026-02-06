#include "replay/replay_engine.h"
#include "gateway/frame_recorder.h"
#include "gateway/telemetry_parser.h"
#include "gateway/sequence_tracker.h"
#include "gateway/stats_manager.h"
#include "sensor_sim/object_generator.h"
#include "sensor_sim/world_model.h"
#include "sensor_sim/measurement_generator.h"
#include "sensor_sim/fault_injector.h"
#include <gtest/gtest.h>
#include <cstdio>

using namespace nng;

class ReplayDeterminismTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = "/tmp/test_replay_determinism_" + std::to_string(rand()) + ".bin";
    }

    void TearDown() override {
        std::remove(test_file_.c_str());
    }

    std::string test_file_;
};

TEST_F(ReplayDeterminismTest, LiveVsReplayStatsMatch) {
    GlobalStats live_stats;
    GlobalStats replay_stats;

    // Phase 1: Run live pipeline and record frames
    {
        // Create pipeline components
        auto profile = profile_patrol();
        ObjectGenerator generator(profile, 42);
        WorldModel world;
        MeasurementGenerator measurer(1, 123);
        FaultConfig fault_config;
        fault_config.loss_pct = 5.0;
        fault_config.reorder_pct = 2.0;
        FaultInjector injector(fault_config, 99);
        SequenceTracker tracker;
        StatsManager stats;
        FrameRecorder recorder;

        ASSERT_TRUE(recorder.open(test_file_));

        // Initialize world
        auto initial_objects = generator.generate_initial();
        for (auto& obj : initial_objects) {
            world.add_object(obj);
        }

        uint64_t timestamp_ns = 0;
        const double dt = 0.02; // 50 Hz
        const int num_ticks = 100;

        for (int tick = 0; tick < num_ticks; ++tick) {
            double current_time_s = tick * dt;
            timestamp_ns = static_cast<uint64_t>(current_time_s * 1e9);

            // Maybe spawn new object
            auto spawned = generator.maybe_spawn(current_time_s);
            if (spawned) {
                world.add_object(*spawned);
            }

            // Tick world
            world.tick(dt, current_time_s);

            // Generate frames
            auto tracks = measurer.generate_tracks(world.objects(), timestamp_ns);
            auto plots = measurer.generate_plots(world.objects(), timestamp_ns);

            // Combine frames
            std::vector<std::vector<uint8_t>> frames;
            frames.insert(frames.end(), tracks.begin(), tracks.end());
            frames.insert(frames.end(), plots.begin(), plots.end());

            // Apply faults
            injector.apply(frames);

            // Process frames and record
            for (const auto& frame : frames) {
                // Record for replay
                recorder.record(timestamp_ns, frame.data(), frame.size());

                // Process through pipeline
                ParsedFrame parsed;
                auto err = parse_frame(frame.data(), frame.size(), false, parsed);
                if (err == ParseError::OK) {
                    auto seq_result = tracker.track(parsed.header.src_id, parsed.header.seq);
                    stats.record_rx(parsed.header.src_id, parsed.header.seq, timestamp_ns);

                    if (seq_result.result == SeqResult::GAP) {
                        stats.record_gap(parsed.header.src_id, seq_result.gap_size);
                    } else if (seq_result.result == SeqResult::REORDER) {
                        stats.record_reorder(parsed.header.src_id);
                    } else if (seq_result.result == SeqResult::DUPLICATE) {
                        stats.record_duplicate(parsed.header.src_id);
                    }
                } else {
                    stats.record_malformed(0);
                }
            }
        }

        recorder.close();
        live_stats = stats.get_global_stats();
    }

    // Phase 2: Replay recorded frames through same pipeline
    {
        SequenceTracker tracker;
        StatsManager stats;
        ReplayFrameSource replay;

        ASSERT_TRUE(replay.open(test_file_));
        replay.set_speed(0.0); // As fast as possible

        std::vector<uint8_t> frame;
        while (!replay.is_done()) {
            if (!replay.receive(frame))
                break;

            ParsedFrame parsed;
            auto err = parse_frame(frame.data(), frame.size(), false, parsed);
            if (err == ParseError::OK) {
                auto seq_result = tracker.track(parsed.header.src_id, parsed.header.seq);
                stats.record_rx(parsed.header.src_id, parsed.header.seq, 0);

                if (seq_result.result == SeqResult::GAP) {
                    stats.record_gap(parsed.header.src_id, seq_result.gap_size);
                } else if (seq_result.result == SeqResult::REORDER) {
                    stats.record_reorder(parsed.header.src_id);
                } else if (seq_result.result == SeqResult::DUPLICATE) {
                    stats.record_duplicate(parsed.header.src_id);
                }
            } else {
                stats.record_malformed(0);
            }
        }

        replay_stats = stats.get_global_stats();
    }

    // Phase 3: Compare stats
    EXPECT_EQ(live_stats.rx_total, replay_stats.rx_total)
        << "rx_total mismatch: live=" << live_stats.rx_total
        << " replay=" << replay_stats.rx_total;

    EXPECT_EQ(live_stats.gap_total, replay_stats.gap_total)
        << "gap_total mismatch: live=" << live_stats.gap_total
        << " replay=" << replay_stats.gap_total;

    EXPECT_EQ(live_stats.reorder_total, replay_stats.reorder_total)
        << "reorder_total mismatch: live=" << live_stats.reorder_total
        << " replay=" << replay_stats.reorder_total;

    EXPECT_EQ(live_stats.duplicate_total, replay_stats.duplicate_total)
        << "duplicate_total mismatch: live=" << live_stats.duplicate_total
        << " replay=" << replay_stats.duplicate_total;

    EXPECT_EQ(live_stats.malformed_total, replay_stats.malformed_total)
        << "malformed_total mismatch: live=" << live_stats.malformed_total
        << " replay=" << replay_stats.malformed_total;

    // Verify we processed some frames
    EXPECT_GT(live_stats.rx_total, 0u) << "Expected some frames to be processed";
}

TEST_F(ReplayDeterminismTest, MultipleReplaysIdentical) {
    // Record some frames
    {
        FrameRecorder recorder;
        ASSERT_TRUE(recorder.open(test_file_));

        auto profile = profile_patrol();
        ObjectGenerator generator(profile, 42);
        WorldModel world;
        MeasurementGenerator measurer(1, 123);

        auto initial = generator.generate_initial();
        for (auto& obj : initial) {
            world.add_object(obj);
        }

        for (int tick = 0; tick < 50; ++tick) {
            double t = tick * 0.02;
            world.tick(0.02, t);
            auto tracks = measurer.generate_tracks(world.objects(),
                static_cast<uint64_t>(t * 1e9));
            for (const auto& frame : tracks) {
                recorder.record(static_cast<uint64_t>(t * 1e9), frame.data(), frame.size());
            }
        }
        recorder.close();
    }

    // Replay twice and compare
    auto replay_and_collect = [this]() {
        std::vector<std::vector<uint8_t>> frames;
        ReplayFrameSource replay;
        EXPECT_TRUE(replay.open(test_file_));
        replay.set_speed(0.0);

        std::vector<uint8_t> buf;
        while (!replay.is_done()) {
            if (replay.receive(buf)) {
                frames.push_back(buf);
            }
        }
        return frames;
    };

    auto frames1 = replay_and_collect();
    auto frames2 = replay_and_collect();

    ASSERT_EQ(frames1.size(), frames2.size());
    for (size_t i = 0; i < frames1.size(); ++i) {
        EXPECT_EQ(frames1[i], frames2[i]) << "Frame " << i << " differs between replays";
    }
}
