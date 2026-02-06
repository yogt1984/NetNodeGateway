#include "control_node/command_handler.h"
#include "gateway/stats_manager.h"
#include "common/logger.h"
#include <gtest/gtest.h>
#include <sstream>

using namespace nng;

class CommandHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        log_stream_ = std::make_unique<std::ostringstream>();
        Logger::instance().set_output(*log_stream_);
        stats_ = std::make_unique<StatsManager>();
        handler_ = std::make_unique<CommandHandler>(*stats_, Logger::instance());
    }

    std::unique_ptr<std::ostringstream> log_stream_;
    std::unique_ptr<StatsManager> stats_;
    std::unique_ptr<CommandHandler> handler_;
};

TEST_F(CommandHandlerTest, GetStats) {
    // Record some statistics
    stats_->record_rx(1, 1, 1000);
    stats_->record_rx(1, 2, 2000);
    stats_->record_malformed(1);

    std::string response = handler_->handle("GET stats");

    EXPECT_NE(response.find("STATS"), std::string::npos);
    EXPECT_NE(response.find("rx_total=2"), std::string::npos);
}

TEST_F(CommandHandlerTest, GetHealth) {
    std::string response = handler_->handle("GET health");

    // Health state is OK by default
    EXPECT_NE(response.find("HEALTH"), std::string::npos);
    EXPECT_NE(response.find("OK"), std::string::npos);
}

TEST_F(CommandHandlerTest, CrcEnabledByDefault) {
    EXPECT_TRUE(handler_->crc_enabled());
}

TEST_F(CommandHandlerTest, SetCrcDisable) {
    EXPECT_TRUE(handler_->crc_enabled());

    std::string response = handler_->handle("SET CRC=OFF");
    EXPECT_NE(response.find("OK"), std::string::npos);
    EXPECT_NE(response.find("CRC=OFF"), std::string::npos);
    EXPECT_FALSE(handler_->crc_enabled());
}

TEST_F(CommandHandlerTest, SetCrcEnable) {
    handler_->handle("SET CRC=OFF");
    EXPECT_FALSE(handler_->crc_enabled());

    std::string response = handler_->handle("SET CRC=ON");
    EXPECT_NE(response.find("OK"), std::string::npos);
    EXPECT_NE(response.find("CRC=ON"), std::string::npos);
    EXPECT_TRUE(handler_->crc_enabled());
}

TEST_F(CommandHandlerTest, UnknownCommand) {
    std::string response = handler_->handle("INVALID command");
    EXPECT_NE(response.find("ERR"), std::string::npos);
}

TEST_F(CommandHandlerTest, UnknownGetKey) {
    std::string response = handler_->handle("GET unknown_key");
    EXPECT_NE(response.find("ERR"), std::string::npos);
}

TEST_F(CommandHandlerTest, GenericSetKey) {
    // Generic key-value storage works for unknown keys
    std::string response = handler_->handle("SET MYKEY=MYVALUE");
    EXPECT_NE(response.find("OK"), std::string::npos);
    EXPECT_EQ(handler_->get_config("MYKEY"), "MYVALUE");
}

TEST_F(CommandHandlerTest, EmptyCommand) {
    std::string response = handler_->handle("");
    EXPECT_NE(response.find("ERR"), std::string::npos);
}

TEST_F(CommandHandlerTest, GetWithoutKey) {
    std::string response = handler_->handle("GET");
    EXPECT_NE(response.find("ERR"), std::string::npos);
}

TEST_F(CommandHandlerTest, SetWithoutValue) {
    std::string response = handler_->handle("SET crc_enabled");
    EXPECT_NE(response.find("ERR"), std::string::npos);
}

TEST_F(CommandHandlerTest, CaseInsensitiveCommand) {
    std::string response1 = handler_->handle("get health");
    std::string response2 = handler_->handle("GET health");

    // Both should work
    EXPECT_NE(response1.find("HEALTH"), std::string::npos);
    EXPECT_NE(response2.find("HEALTH"), std::string::npos);
}
