#include "cli/cli_client.h"
#include "control_node/control_node.h"
#include "gateway/stats_manager.h"
#include "common/logger.h"
#include <gtest/gtest.h>
#include <sstream>
#include <thread>
#include <chrono>

using namespace nng;

class TcpLoopbackTest : public ::testing::Test {
protected:
    void SetUp() override {
        log_stream_ = std::make_unique<std::ostringstream>();
        Logger::instance().set_output(*log_stream_);
        stats_ = std::make_unique<StatsManager>();
    }

    std::unique_ptr<std::ostringstream> log_stream_;
    std::unique_ptr<StatsManager> stats_;
};

TEST_F(TcpLoopbackTest, ServerStartStop) {
    ControlNode node(19900, *stats_, Logger::instance());

    EXPECT_FALSE(node.is_running());
    EXPECT_TRUE(node.start());
    EXPECT_TRUE(node.is_running());

    node.stop();
    EXPECT_FALSE(node.is_running());
}

TEST_F(TcpLoopbackTest, ClientConnect) {
    ControlNode node(19901, *stats_, Logger::instance());
    ASSERT_TRUE(node.start());

    // Give server time to start listening
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    CliClient client;
    EXPECT_FALSE(client.is_connected());
    EXPECT_TRUE(client.connect("127.0.0.1", 19901));
    EXPECT_TRUE(client.is_connected());

    client.close();
    EXPECT_FALSE(client.is_connected());

    node.stop();
}

TEST_F(TcpLoopbackTest, SendCommand) {
    ControlNode node(19902, *stats_, Logger::instance());
    ASSERT_TRUE(node.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    CliClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", 19902));

    std::string response = client.send_command("GET health");
    EXPECT_NE(response.find("HEALTH"), std::string::npos);
    EXPECT_NE(response.find("OK"), std::string::npos);

    client.close();
    node.stop();
}

TEST_F(TcpLoopbackTest, MultipleCommands) {
    ControlNode node(19903, *stats_, Logger::instance());
    ASSERT_TRUE(node.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    CliClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", 19903));

    // Multiple sequential commands
    std::string r1 = client.send_command("GET health");
    EXPECT_NE(r1.find("HEALTH"), std::string::npos);

    std::string r2 = client.send_command("GET stats");
    EXPECT_NE(r2.find("rx_total"), std::string::npos);

    std::string r3 = client.send_command("SET CRC=OFF");
    EXPECT_NE(r3.find("OK"), std::string::npos);

    std::string r4 = client.send_command("SET CRC=ON");
    EXPECT_NE(r4.find("OK"), std::string::npos);

    client.close();
    node.stop();
}

TEST_F(TcpLoopbackTest, MultipleClients) {
    ControlNode node(19904, *stats_, Logger::instance());
    ASSERT_TRUE(node.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    CliClient client1, client2;
    ASSERT_TRUE(client1.connect("127.0.0.1", 19904));
    ASSERT_TRUE(client2.connect("127.0.0.1", 19904));

    std::string r1 = client1.send_command("GET health");
    std::string r2 = client2.send_command("GET health");

    EXPECT_NE(r1.find("HEALTH"), std::string::npos);
    EXPECT_NE(r2.find("HEALTH"), std::string::npos);

    client1.close();
    client2.close();
    node.stop();
}

TEST_F(TcpLoopbackTest, InvalidCommand) {
    ControlNode node(19905, *stats_, Logger::instance());
    ASSERT_TRUE(node.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    CliClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", 19905));

    std::string response = client.send_command("INVALID xyz");
    EXPECT_NE(response.find("ERR"), std::string::npos);

    client.close();
    node.stop();
}

TEST_F(TcpLoopbackTest, GetStats) {
    ControlNode node(19906, *stats_, Logger::instance());
    ASSERT_TRUE(node.start());

    // Record some stats
    stats_->record_rx(1, 1, 1000);
    stats_->record_rx(1, 2, 2000);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    CliClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", 19906));

    std::string response = client.send_command("GET stats");
    EXPECT_NE(response.find("rx_total"), std::string::npos);

    client.close();
    node.stop();
}

TEST_F(TcpLoopbackTest, ClientConnectFail) {
    CliClient client;

    // Try to connect to non-listening port
    EXPECT_FALSE(client.connect("127.0.0.1", 19999));
    EXPECT_FALSE(client.is_connected());
}

TEST_F(TcpLoopbackTest, SendWithoutConnect) {
    CliClient client;

    std::string response = client.send_command("GET health");
    EXPECT_TRUE(response.empty());
}
