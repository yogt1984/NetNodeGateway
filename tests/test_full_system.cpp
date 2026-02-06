#include "gateway/gateway.h"
#include "control_node/control_node.h"
#include "cli/cli_client.h"
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

class FullSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        record_file_ = "/tmp/test_full_system_" + std::to_string(rand()) + ".bin";
        log_stream_ = std::make_unique<std::ostringstream>();
        Logger::instance().set_output(*log_stream_);
        Logger::instance().set_level(Severity::DEBUG);
    }

    void TearDown() override {
        std::remove(record_file_.c_str());
    }

    std::string record_file_;
    std::unique_ptr<std::ostringstream> log_stream_;
};

TEST_F(FullSystemTest, GatewayReceivesFrames) {
    const uint16_t udp_port = 17000;
    const uint16_t tcp_port = 17001;

    // Configure gateway with recording
    GatewayConfig gw_config;
    gw_config.udp_port = udp_port;
    gw_config.crc_enabled = false;  // Sensor sim doesn't add CRC
    gw_config.record_enabled = true;
    gw_config.record_path = record_file_;
    gw_config.log_level = Severity::DEBUG;

    Gateway gateway(gw_config);
    StatsManager stats_for_control;
    ControlNode control(tcp_port, stats_for_control, Logger::instance());

    // Start gateway in a thread
    std::thread gateway_thread([&gateway]() {
        gateway.run();
    });

    // Start control node
    ASSERT_TRUE(control.start());

    // Give gateway time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Start sensor sim in a thread
    std::atomic<bool> sensor_done{false};
    std::thread sensor_thread([&sensor_done, udp_port]() {
        auto profile = profile_patrol();
        ObjectGenerator generator(profile, 42);
        WorldModel world;
        MeasurementGenerator measurer(1, 123);
        UdpFrameSink sink;

        if (!sink.connect("127.0.0.1", udp_port)) {
            return;
        }

        auto initial = generator.generate_initial();
        for (auto& obj : initial) {
            world.add_object(obj);
        }

        const double dt = 0.02;  // 50 Hz
        const int ticks = 150;   // 3 seconds

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

            if (tick % 50 == 0) {
                auto hb = measurer.generate_heartbeat(ts);
                sink.send(hb);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        sink.close();
        sensor_done.store(true);
    });

    // Wait for sensor sim to finish
    sensor_thread.join();

    // Wait a bit for gateway to process remaining frames
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Query stats via CLI
    CliClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", tcp_port));

    // Get stats - use gateway's stats directly since control node has separate stats
    auto gw_stats = gateway.stats().get_global_stats();

    // Get health from control
    std::string health_response = client.send_command("GET health");
    EXPECT_TRUE(health_response.find("HEALTH") != std::string::npos);

    client.close();

    // Stop gateway and control
    gateway.stop();
    gateway_thread.join();
    control.stop();

    // Verify stats
    EXPECT_GT(gw_stats.rx_total, 50u)
        << "Expected more than 50 frames, got " << gw_stats.rx_total;

    // Verify recorded file exists and has size > 0
    std::ifstream check(record_file_, std::ios::binary | std::ios::ate);
    EXPECT_TRUE(check.is_open());
    EXPECT_GT(check.tellg(), 0);
}

TEST_F(FullSystemTest, GatewayHealthQuery) {
    const uint16_t udp_port = 17010;
    const uint16_t tcp_port = 17011;

    GatewayConfig gw_config;
    gw_config.udp_port = udp_port;
    gw_config.crc_enabled = false;
    gw_config.log_level = Severity::WARN;

    Gateway gateway(gw_config);
    StatsManager stats_for_control;
    ControlNode control(tcp_port, stats_for_control, Logger::instance());

    std::thread gateway_thread([&gateway]() {
        gateway.run();
    });

    ASSERT_TRUE(control.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Query health
    CliClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", tcp_port));

    std::string response = client.send_command("GET health");
    EXPECT_TRUE(response.find("HEALTH") != std::string::npos);
    // Should be OK or DEGRADED (not ERROR without any traffic issues)
    EXPECT_TRUE(response.find("OK") != std::string::npos ||
                response.find("DEGRADED") != std::string::npos);

    client.close();
    gateway.stop();
    gateway_thread.join();
    control.stop();
}
