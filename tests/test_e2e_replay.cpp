#include "gateway/gateway.h"
#include "replay/replay_engine.h"
#include "sensor_sim/object_generator.h"
#include "sensor_sim/world_model.h"
#include "sensor_sim/measurement_generator.h"
#include "gateway/udp_socket.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <cstdio>

using namespace nng;

class E2EReplayTest : public ::testing::Test {
protected:
    void SetUp() override {
        record_file_ = "/tmp/test_e2e_replay_" + std::to_string(rand()) + ".bin";
        log_stream_ = std::make_unique<std::ostringstream>();
        Logger::instance().set_output(*log_stream_);
        Logger::instance().set_level(Severity::WARN);
    }

    void TearDown() override {
        std::remove(record_file_.c_str());
    }

    std::string record_file_;
    std::unique_ptr<std::ostringstream> log_stream_;
};

TEST_F(E2EReplayTest, LiveAndReplayStatsMatch) {
    GlobalStats live_stats;
    GlobalStats replay_stats;

    const uint16_t udp_port = 17030;

    // Phase 1: Run live system with recording
    {
        GatewayConfig gw_config;
        gw_config.udp_port = udp_port;
        gw_config.crc_enabled = false;
        gw_config.record_enabled = true;
        gw_config.record_path = record_file_;
        gw_config.log_level = Severity::WARN;

        Gateway gateway(gw_config);

        std::thread gateway_thread([&gateway]() {
            gateway.run();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Run sensor sim
        {
            auto profile = profile_patrol();
            ObjectGenerator generator(profile, 42);
            WorldModel world;
            MeasurementGenerator measurer(1, 123);
            UdpFrameSink sink;

            ASSERT_TRUE(sink.connect("127.0.0.1", udp_port));

            auto initial = generator.generate_initial();
            for (auto& obj : initial) {
                world.add_object(obj);
            }

            const double dt = 0.02;
            const int ticks = 100;  // 2 seconds

            for (int tick = 0; tick < ticks; ++tick) {
                double t = tick * dt;
                uint64_t ts = static_cast<uint64_t>(t * 1e9);

                auto spawned = generator.maybe_spawn(t);
                if (spawned) world.add_object(*spawned);

                world.tick(dt, t);

                auto tracks = measurer.generate_tracks(world.objects(), ts);
                auto plots = measurer.generate_plots(world.objects(), ts);

                for (const auto& frame : tracks) sink.send(frame);
                for (const auto& frame : plots) sink.send(frame);

                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            sink.close();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        gateway.stop();
        gateway_thread.join();

        live_stats = gateway.stats().get_global_stats();
    }

    // Phase 2: Replay recorded file through gateway
    {
        GatewayConfig gw_config;
        gw_config.crc_enabled = false;
        gw_config.replay_path = record_file_;
        gw_config.log_level = Severity::WARN;

        Gateway gateway(gw_config);
        gateway.run();  // Will run until replay is done

        replay_stats = gateway.stats().get_global_stats();
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

TEST_F(E2EReplayTest, MultipleReplaysProduceSameStats) {
    const uint16_t udp_port = 17031;

    // Record some frames
    {
        GatewayConfig gw_config;
        gw_config.udp_port = udp_port;
        gw_config.crc_enabled = false;
        gw_config.record_enabled = true;
        gw_config.record_path = record_file_;

        Gateway gateway(gw_config);

        std::thread gateway_thread([&gateway]() {
            gateway.run();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Send some frames
        {
            MeasurementGenerator measurer(1, 123);
            UdpFrameSink sink;
            ASSERT_TRUE(sink.connect("127.0.0.1", udp_port));

            WorldObject obj;
            obj.id = 1;
            obj.classification = TrackClass::FIXED_WING;
            obj.azimuth_deg = 45.0;
            obj.elevation_deg = 10.0;
            obj.range_m = 10000.0;
            obj.speed_mps = 200.0;
            obj.heading_deg = 270.0;
            obj.rcs_dbsm = 10.0;
            obj.is_hostile = false;
            obj.noise_stddev = 1.0;

            std::vector<WorldObject> objects = {obj};

            for (int i = 0; i < 50; ++i) {
                auto tracks = measurer.generate_tracks(objects, i * 20000000);
                for (const auto& frame : tracks) sink.send(frame);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            sink.close();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        gateway.stop();
        gateway_thread.join();
    }

    // Replay twice
    auto replay_and_get_stats = [this]() {
        GatewayConfig gw_config;
        gw_config.crc_enabled = false;
        gw_config.replay_path = record_file_;

        Gateway gateway(gw_config);
        gateway.run();
        return gateway.stats().get_global_stats();
    };

    auto stats1 = replay_and_get_stats();
    auto stats2 = replay_and_get_stats();

    EXPECT_EQ(stats1.rx_total, stats2.rx_total);
    EXPECT_EQ(stats1.gap_total, stats2.gap_total);
    EXPECT_EQ(stats1.reorder_total, stats2.reorder_total);
    EXPECT_EQ(stats1.malformed_total, stats2.malformed_total);
}
