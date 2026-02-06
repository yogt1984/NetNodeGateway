#include "common/logger.h"
#include "common/types.h"
#include <gtest/gtest.h>
#include <sstream>
#include <regex>
#include <set>

using namespace nng;

class LogFormatTest : public ::testing::Test {
protected:
    void SetUp() override {
        log_stream_ = std::make_unique<std::ostringstream>();
        Logger::instance().set_output(*log_stream_);
        Logger::instance().set_level(Severity::DEBUG);
    }

    std::unique_ptr<std::ostringstream> log_stream_;
};

TEST_F(LogFormatTest, LogLineMatchesFormat) {
    // Log some messages
    Logger::instance().log(Severity::INFO, EventCategory::TRACKING,
        "EVT_TRACK_NEW", "src_id=1 track_id=100");
    Logger::instance().log(Severity::DEBUG, EventCategory::NETWORK,
        "EVT_SOURCE_ONLINE", "src_id=2");
    Logger::instance().log(Severity::WARN, EventCategory::HEALTH,
        "EVT_HEARTBEAT_DEGRADE", "subsystem=1 cpu=95%");
    Logger::instance().log(Severity::ALARM, EventCategory::THREAT,
        "EVT_THREAT_CRITICAL", "track_id=42 level=4");
    Logger::instance().log(Severity::ERROR, EventCategory::ENGAGEMENT,
        "EVT_WEAPON_STATUS", "weapon=1 mode=2");

    std::string output = log_stream_->str();

    // Expected format:
    // YYYY-MM-DDTHH:MM:SS.mmmZ [SEVER] [CATEGORY  ] EVENT_NAME          detail
    // The regex pattern:
    // ^\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}\\.\\d{3}Z \\[(DEBUG|INFO |WARN |ALARM|ERROR|FATAL)\\] \\[.{10}\\] .{20}.+$

    std::regex line_pattern(
        R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z \[(DEBUG|INFO |WARN |ALARM|ERROR|FATAL)\] \[.{10}\] .{20}.+$)"
    );

    std::istringstream iss(output);
    std::string line;
    int line_count = 0;
    int match_count = 0;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        line_count++;

        if (std::regex_match(line, line_pattern)) {
            match_count++;
        } else {
            ADD_FAILURE() << "Line does not match expected format: " << line;
        }
    }

    EXPECT_EQ(line_count, 5);
    EXPECT_EQ(match_count, 5);
}

TEST_F(LogFormatTest, TimestampIsValidISO8601) {
    Logger::instance().log(Severity::INFO, EventCategory::CONTROL,
        "EVT_CONFIG_CHANGE", "test");

    std::string output = log_stream_->str();

    // Extract timestamp (first 24 characters: YYYY-MM-DDTHH:MM:SS.mmmZ)
    ASSERT_GE(output.size(), 24u);
    std::string timestamp = output.substr(0, 24);

    // Verify format
    std::regex ts_pattern(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z)");
    EXPECT_TRUE(std::regex_match(timestamp, ts_pattern))
        << "Timestamp doesn't match ISO 8601: " << timestamp;

    // Verify it ends with Z (UTC)
    EXPECT_EQ(timestamp.back(), 'Z');
}

TEST_F(LogFormatTest, SeverityPadding) {
    Logger::instance().log(Severity::DEBUG, EventCategory::CONTROL, "TEST", "");
    Logger::instance().log(Severity::INFO, EventCategory::CONTROL, "TEST", "");
    Logger::instance().log(Severity::WARN, EventCategory::CONTROL, "TEST", "");
    Logger::instance().log(Severity::ALARM, EventCategory::CONTROL, "TEST", "");
    Logger::instance().log(Severity::ERROR, EventCategory::CONTROL, "TEST", "");
    Logger::instance().log(Severity::FATAL, EventCategory::CONTROL, "TEST", "");

    std::string output = log_stream_->str();

    // Each severity should be exactly 5 characters (padded with space)
    EXPECT_NE(output.find("[DEBUG]"), std::string::npos);
    EXPECT_NE(output.find("[INFO ]"), std::string::npos);
    EXPECT_NE(output.find("[WARN ]"), std::string::npos);
    EXPECT_NE(output.find("[ALARM]"), std::string::npos);
    EXPECT_NE(output.find("[ERROR]"), std::string::npos);
    EXPECT_NE(output.find("[FATAL]"), std::string::npos);
}

TEST_F(LogFormatTest, CategoryPadding) {
    Logger::instance().log(Severity::INFO, EventCategory::TRACKING, "TEST", "");
    Logger::instance().log(Severity::INFO, EventCategory::THREAT, "TEST", "");
    Logger::instance().log(Severity::INFO, EventCategory::IFF, "TEST", "");
    Logger::instance().log(Severity::INFO, EventCategory::ENGAGEMENT, "TEST", "");
    Logger::instance().log(Severity::INFO, EventCategory::NETWORK, "TEST", "");
    Logger::instance().log(Severity::INFO, EventCategory::HEALTH, "TEST", "");
    Logger::instance().log(Severity::INFO, EventCategory::CONTROL, "TEST", "");

    std::string output = log_stream_->str();

    // Each category should be exactly 10 characters (padded)
    // The pattern [XXXXXXXXXX] where X is the category name padded to 10 chars
    std::regex cat_pattern(R"(\[.{10}\])");

    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        // Find the category bracket (second bracket after severity)
        size_t first_bracket = line.find('[');
        ASSERT_NE(first_bracket, std::string::npos);
        size_t second_bracket = line.find('[', first_bracket + 1);
        ASSERT_NE(second_bracket, std::string::npos);

        // Extract category portion
        std::string cat_portion = line.substr(second_bracket, 12);  // [XXXXXXXXXX]
        EXPECT_TRUE(std::regex_match(cat_portion, cat_pattern))
            << "Category not padded correctly: " << cat_portion;
    }
}

TEST_F(LogFormatTest, MultipleEventTypes) {
    // Log different event types
    Logger::instance().log(Severity::INFO, EventCategory::TRACKING, "EVT_TRACK_NEW", "");
    Logger::instance().log(Severity::INFO, EventCategory::TRACKING, "EVT_TRACK_UPDATE", "");
    Logger::instance().log(Severity::WARN, EventCategory::NETWORK, "EVT_SEQ_GAP", "");
    Logger::instance().log(Severity::INFO, EventCategory::HEALTH, "EVT_HEARTBEAT_OK", "");
    Logger::instance().log(Severity::INFO, EventCategory::ENGAGEMENT, "EVT_WEAPON_STATUS", "");
    Logger::instance().log(Severity::INFO, EventCategory::CONTROL, "EVT_CONFIG_CHANGE", "");

    std::string output = log_stream_->str();

    // Verify at least 5 different event types
    std::set<std::string> event_types;
    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        // Event name starts after the second ']' and a space
        size_t pos = line.find(']');
        if (pos != std::string::npos) {
            pos = line.find(']', pos + 1);
            if (pos != std::string::npos && pos + 2 < line.size()) {
                std::string event_name = line.substr(pos + 2, 20);
                // Trim trailing spaces
                size_t end = event_name.find_last_not_of(' ');
                if (end != std::string::npos) {
                    event_name = event_name.substr(0, end + 1);
                }
                event_types.insert(event_name);
            }
        }
    }

    EXPECT_GE(event_types.size(), 5u)
        << "Expected at least 5 different event types, got " << event_types.size();
}

TEST_F(LogFormatTest, EventNamePadding) {
    Logger::instance().log(Severity::INFO, EventCategory::TRACKING, "EVT_TRACK_NEW", "detail");

    std::string output = log_stream_->str();

    // Event name should be padded to 20 characters
    // Find the event name position (after second bracket)
    size_t pos = output.find(']');
    ASSERT_NE(pos, std::string::npos);
    pos = output.find(']', pos + 1);
    ASSERT_NE(pos, std::string::npos);

    // Event name starts at pos + 2 and should be 20 chars
    std::string event_portion = output.substr(pos + 2, 20);
    EXPECT_EQ(event_portion.size(), 20u);

    // Should start with EVT_TRACK_NEW and be padded
    EXPECT_TRUE(event_portion.find("EVT_TRACK_NEW") == 0);
}
