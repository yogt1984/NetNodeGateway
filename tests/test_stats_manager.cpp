#include <gtest/gtest.h>
#include "gateway/stats_manager.h"
#include <thread>
#include <vector>

using namespace nng;

class StatsManagerTest : public ::testing::Test {
protected:
    StatsManager sm;
};

TEST_F(StatsManagerTest, InitiallyZero) {
    auto g = sm.get_global_stats();
    EXPECT_EQ(g.rx_total, 0u);
    EXPECT_EQ(g.malformed_total, 0u);
    EXPECT_EQ(g.gap_total, 0u);
    EXPECT_EQ(g.reorder_total, 0u);
    EXPECT_EQ(g.duplicate_total, 0u);
    EXPECT_EQ(g.crc_fail_total, 0u);
}

TEST_F(StatsManagerTest, RecordRxIncrementsGlobal) {
    for (int i = 0; i < 10; ++i)
        sm.record_rx(1, i, i * 1000);
    EXPECT_EQ(sm.get_global_stats().rx_total, 10u);
}

TEST_F(StatsManagerTest, RecordRxPerSourceCorrect) {
    sm.record_rx(1, 0, 100);
    sm.record_rx(1, 1, 200);
    sm.record_rx(2, 0, 100);

    auto s1 = sm.get_source_stats(1);
    EXPECT_EQ(s1.src_id, 1);
    EXPECT_EQ(s1.rx_count, 2u);
    EXPECT_EQ(s1.last_seq, 1u);
    EXPECT_EQ(s1.last_ts_ns, 200u);

    auto s2 = sm.get_source_stats(2);
    EXPECT_EQ(s2.rx_count, 1u);
}

TEST_F(StatsManagerTest, RecordGapIncrementsGlobalAndSource) {
    sm.record_gap(1, 3);
    sm.record_gap(1, 2);
    sm.record_gap(2, 5);

    EXPECT_EQ(sm.get_global_stats().gap_total, 10u);
    EXPECT_EQ(sm.get_source_stats(1).gaps, 5u);
    EXPECT_EQ(sm.get_source_stats(2).gaps, 5u);
}

TEST_F(StatsManagerTest, RecordMalformedIncrements) {
    sm.record_malformed(1);
    sm.record_malformed(1);
    sm.record_malformed(2);
    EXPECT_EQ(sm.get_global_stats().malformed_total, 3u);
    EXPECT_EQ(sm.get_source_stats(1).malformed, 2u);
}

TEST_F(StatsManagerTest, RecordReorderIncrements) {
    sm.record_reorder(1);
    EXPECT_EQ(sm.get_global_stats().reorder_total, 1u);
    EXPECT_EQ(sm.get_source_stats(1).reorders, 1u);
}

TEST_F(StatsManagerTest, RecordDuplicateIncrements) {
    sm.record_duplicate(1);
    sm.record_duplicate(1);
    EXPECT_EQ(sm.get_global_stats().duplicate_total, 2u);
    EXPECT_EQ(sm.get_source_stats(1).duplicates, 2u);
}

TEST_F(StatsManagerTest, RecordCrcFailIncrementsBothCounters) {
    sm.record_crc_fail(1);
    EXPECT_EQ(sm.get_global_stats().crc_fail_total, 1u);
    // CRC fail also counts as malformed on the source
    EXPECT_EQ(sm.get_source_stats(1).malformed, 1u);
}

TEST_F(StatsManagerTest, ResetClearsEverything) {
    sm.record_rx(1, 0, 100);
    sm.record_gap(1, 5);
    sm.record_malformed(2);
    sm.reset();

    auto g = sm.get_global_stats();
    EXPECT_EQ(g.rx_total, 0u);
    EXPECT_EQ(g.gap_total, 0u);
    EXPECT_EQ(g.malformed_total, 0u);

    auto all = sm.get_all_source_stats();
    EXPECT_TRUE(all.empty());
}

TEST_F(StatsManagerTest, HealthOkWhenClean) {
    EXPECT_EQ(sm.get_health(), HealthState::OK);
}

TEST_F(StatsManagerTest, HealthDegradedAfterGaps) {
    sm.record_gap(1, 1);
    EXPECT_EQ(sm.get_health(), HealthState::DEGRADED);
}

TEST_F(StatsManagerTest, HealthDegradedAfterReorder) {
    sm.record_reorder(1);
    EXPECT_EQ(sm.get_health(), HealthState::DEGRADED);
}

TEST_F(StatsManagerTest, HealthErrorAfterMalformed) {
    sm.record_malformed(1);
    EXPECT_EQ(sm.get_health(), HealthState::ERROR);
}

TEST_F(StatsManagerTest, HealthErrorAfterCrcFail) {
    sm.record_crc_fail(1);
    EXPECT_EQ(sm.get_health(), HealthState::ERROR);
}

TEST_F(StatsManagerTest, HealthErrorTakesPrecedenceOverDegraded) {
    sm.record_gap(1, 5);       // would be DEGRADED
    sm.record_malformed(1);    // should push to ERROR
    EXPECT_EQ(sm.get_health(), HealthState::ERROR);
}

TEST_F(StatsManagerTest, GetAllSourceStats) {
    sm.record_rx(10, 0, 100);
    sm.record_rx(20, 0, 100);
    sm.record_rx(30, 0, 100);

    auto all = sm.get_all_source_stats();
    EXPECT_EQ(all.size(), 3u);

    // Verify all src_ids present
    std::set<uint16_t> ids;
    for (const auto& s : all) ids.insert(s.src_id);
    EXPECT_TRUE(ids.count(10));
    EXPECT_TRUE(ids.count(20));
    EXPECT_TRUE(ids.count(30));
}

TEST_F(StatsManagerTest, UnknownSourceReturnsZeroStats) {
    auto s = sm.get_source_stats(999);
    EXPECT_EQ(s.rx_count, 0u);
    EXPECT_EQ(s.src_id, 0);  // default-constructed
}

TEST_F(StatsManagerTest, ThreadSafetyConcurrentWriteAndRead) {
    constexpr int writers = 4;
    constexpr int per_writer = 200;
    std::vector<std::thread> pool;

    for (int w = 0; w < writers; ++w) {
        pool.emplace_back([&, w]() {
            for (int i = 0; i < per_writer; ++i) {
                sm.record_rx(static_cast<uint16_t>(w), i, i * 1000);
                if (i % 10 == 0) sm.record_gap(static_cast<uint16_t>(w), 1);
            }
        });
    }
    // Concurrent reader
    pool.emplace_back([&]() {
        for (int i = 0; i < 100; ++i) {
            auto g = sm.get_global_stats();
            (void)g; // just exercising the read path
            auto all = sm.get_all_source_stats();
            (void)all;
        }
    });

    for (auto& th : pool) th.join();

    EXPECT_EQ(sm.get_global_stats().rx_total,
              static_cast<uint64_t>(writers * per_writer));
}
