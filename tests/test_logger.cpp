#include <gtest/gtest.h>
#include "common/logger.h"
#include <sstream>
#include <regex>
#include <thread>
#include <vector>
#include <cstring>

using namespace nng;

// Fresh logger per test (singleton is for production; tests use a local)
class LoggerTest : public ::testing::Test {
protected:
    Logger& logger = Logger::instance();
    std::ostringstream oss;

    void SetUp() override {
        logger.set_output(oss);
        logger.set_level(Severity::DEBUG);
        oss.str("");
        oss.clear();
    }
};

TEST_F(LoggerTest, InfoMessageMatchesFormat) {
    logger.log(Severity::INFO, EventCategory::TRACKING,
               "EVT_TRACK_NEW", "src=0x0012 track_id=1041");
    std::string line = oss.str();
    ASSERT_FALSE(line.empty()) << "Logger produced no output";

    // Verify the overall structure with a regex
    // Format: YYYY-MM-DDTHH:MM:SS.mmmZ [SEVER] [CATEGORY  ] EVENT_NAME          detail\n
    std::regex pattern(
        R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z \[(DEBUG|INFO |WARN |ALARM|ERROR|FATAL)\] \[.{10}\] .{20}.*\n)");
    EXPECT_TRUE(std::regex_match(line, pattern))
        << "Line does not match expected format: [" << line << "]";
}

TEST_F(LoggerTest, SeverityFilterDropsBelowThreshold) {
    logger.set_level(Severity::WARN);
    logger.log(Severity::INFO, EventCategory::NETWORK, "EVT_SOURCE_ONLINE", "src=0x01");
    EXPECT_TRUE(oss.str().empty())
        << "INFO message should be suppressed when level is WARN";
}

TEST_F(LoggerTest, SeverityFilterPassesAtThreshold) {
    logger.set_level(Severity::WARN);
    logger.log(Severity::WARN, EventCategory::NETWORK, "EVT_SEQ_GAP", "gap=3");
    EXPECT_FALSE(oss.str().empty())
        << "WARN message should pass when level is WARN";
}

TEST_F(LoggerTest, SeverityFilterPassesAboveThreshold) {
    logger.set_level(Severity::WARN);
    logger.log(Severity::ERROR, EventCategory::NETWORK, "EVT_CRC_FAIL", "src=0x18");
    EXPECT_FALSE(oss.str().empty())
        << "ERROR message should pass when level is WARN";
}

TEST_F(LoggerTest, DebugLevelPassesEverything) {
    logger.set_level(Severity::DEBUG);
    logger.log(Severity::DEBUG, EventCategory::HEALTH, "EVT_HEARTBEAT_OK", "cpu=34%");
    EXPECT_FALSE(oss.str().empty());
}

TEST_F(LoggerTest, TimestampIsValidISO8601) {
    logger.log(Severity::INFO, EventCategory::TRACKING, "EVT_TRACK_NEW", "test");
    std::string line = oss.str();
    // Extract timestamp: first 24 chars "YYYY-MM-DDTHH:MM:SS.mmmZ"
    ASSERT_GE(line.size(), 24u);
    std::string ts = line.substr(0, 24);
    EXPECT_EQ(ts[4], '-');
    EXPECT_EQ(ts[7], '-');
    EXPECT_EQ(ts[10], 'T');
    EXPECT_EQ(ts[13], ':');
    EXPECT_EQ(ts[16], ':');
    EXPECT_EQ(ts[19], '.');
    EXPECT_EQ(ts[23], 'Z');

    // Year should be reasonable (2020-2099)
    int year = std::stoi(ts.substr(0, 4));
    EXPECT_GE(year, 2020);
    EXPECT_LE(year, 2099);

    // Month 01-12
    int month = std::stoi(ts.substr(5, 2));
    EXPECT_GE(month, 1);
    EXPECT_LE(month, 12);
}

TEST_F(LoggerTest, CategoryPaddingExactly10Chars) {
    // Each category string must be exactly 10 chars
    EXPECT_EQ(std::strlen(category_str(EventCategory::TRACKING)), 10u);
    EXPECT_EQ(std::strlen(category_str(EventCategory::THREAT)), 10u);
    EXPECT_EQ(std::strlen(category_str(EventCategory::IFF)), 10u);
    EXPECT_EQ(std::strlen(category_str(EventCategory::ENGAGEMENT)), 10u);
    EXPECT_EQ(std::strlen(category_str(EventCategory::NETWORK)), 10u);
    EXPECT_EQ(std::strlen(category_str(EventCategory::HEALTH)), 10u);
    EXPECT_EQ(std::strlen(category_str(EventCategory::CONTROL)), 10u);
}

TEST_F(LoggerTest, SeverityPaddingExactly5Chars) {
    EXPECT_EQ(std::strlen(severity_str(Severity::DEBUG)), 5u);
    EXPECT_EQ(std::strlen(severity_str(Severity::INFO)), 5u);
    EXPECT_EQ(std::strlen(severity_str(Severity::WARN)), 5u);
    EXPECT_EQ(std::strlen(severity_str(Severity::ALARM)), 5u);
    EXPECT_EQ(std::strlen(severity_str(Severity::ERROR)), 5u);
    EXPECT_EQ(std::strlen(severity_str(Severity::FATAL)), 5u);
}

TEST_F(LoggerTest, EventNamePaddedTo20Chars) {
    logger.log(Severity::INFO, EventCategory::IFF, "EVT", "short_name");
    std::string line = oss.str();
    // After "[CATEGORY  ] " the event name should occupy exactly 20 chars
    // Find the second ']'
    auto pos = line.find("] ", line.find("] ") + 1);
    ASSERT_NE(pos, std::string::npos);
    std::string after_cat = line.substr(pos + 2);
    // First 20 chars should include "EVT" plus spaces
    ASSERT_GE(after_cat.size(), 20u);
    std::string evt_field = after_cat.substr(0, 20);
    EXPECT_EQ(evt_field.substr(0, 3), "EVT");
    // Remaining should be spaces
    for (size_t i = 3; i < 20; ++i) {
        EXPECT_EQ(evt_field[i], ' ') << "Position " << i << " should be space";
    }
}

TEST_F(LoggerTest, LongEventNameTruncatedTo20) {
    std::string long_name = "THIS_EVENT_NAME_IS_WAY_TOO_LONG_FOR_FIELD";
    logger.log(Severity::INFO, EventCategory::TRACKING, long_name, "detail");
    std::string line = oss.str();
    // Should not crash, name should be truncated
    EXPECT_FALSE(line.empty());
    // The truncated name should be first 20 chars of long_name
    EXPECT_NE(line.find("THIS_EVENT_NAME_IS_W"), std::string::npos);
}

TEST_F(LoggerTest, EmptyDetailString) {
    logger.log(Severity::WARN, EventCategory::ENGAGEMENT, "EVT_ENGAGE_START", "");
    EXPECT_FALSE(oss.str().empty());
    // Line should still end with newline
    EXPECT_EQ(oss.str().back(), '\n');
}

TEST_F(LoggerTest, MultipleLogLinesAreDistinct) {
    logger.log(Severity::INFO, EventCategory::TRACKING, "EVT_TRACK_NEW", "id=1");
    logger.log(Severity::WARN, EventCategory::NETWORK, "EVT_SEQ_GAP", "gap=5");
    std::string output = oss.str();
    // Count newlines
    auto count = std::count(output.begin(), output.end(), '\n');
    EXPECT_EQ(count, 2) << "Two log calls should produce two lines";
}

TEST_F(LoggerTest, GetLevelReturnsWhatWasSet) {
    logger.set_level(Severity::ALARM);
    EXPECT_EQ(logger.get_level(), Severity::ALARM);
    logger.set_level(Severity::DEBUG);
    EXPECT_EQ(logger.get_level(), Severity::DEBUG);
}

TEST_F(LoggerTest, AllSeveritiesProduceOutput) {
    logger.set_level(Severity::DEBUG);
    Severity levels[] = {Severity::DEBUG, Severity::INFO, Severity::WARN,
                         Severity::ALARM, Severity::ERROR, Severity::FATAL};
    for (auto s : levels) {
        oss.str("");
        oss.clear();
        logger.log(s, EventCategory::CONTROL, "EVT_CONFIG_CHANGE", "test");
        EXPECT_FALSE(oss.str().empty())
            << "Severity " << static_cast<int>(s) << " produced no output";
    }
}

TEST_F(LoggerTest, ThreadSafetyNoCorruption) {
    // Hammer the logger from multiple threads and verify no crash
    logger.set_level(Severity::DEBUG);
    constexpr int threads = 8;
    constexpr int iterations = 100;
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&, t]() {
            for (int i = 0; i < iterations; ++i) {
                logger.log(Severity::INFO, EventCategory::TRACKING,
                           "EVT_TRACK_UPDATE",
                           "thread=" + std::to_string(t) + " i=" + std::to_string(i));
            }
        });
    }
    for (auto& th : pool) th.join();

    std::string output = oss.str();
    auto line_count = std::count(output.begin(), output.end(), '\n');
    EXPECT_EQ(line_count, threads * iterations)
        << "Expected " << threads * iterations << " lines, got " << line_count;
}
