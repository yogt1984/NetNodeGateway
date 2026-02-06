#include <gtest/gtest.h>
#include "gateway/sequence_tracker.h"

using namespace nng;

TEST(SequenceTracker, FirstFrame) {
    SequenceTracker st;
    auto ev = st.track(1, 0);
    EXPECT_EQ(ev.result, SeqResult::FIRST);
    EXPECT_EQ(st.source_count(), 1u);
}

TEST(SequenceTracker, Sequential) {
    SequenceTracker st;
    st.track(1, 0); // FIRST
    EXPECT_EQ(st.track(1, 1).result, SeqResult::OK);
    EXPECT_EQ(st.track(1, 2).result, SeqResult::OK);
    EXPECT_EQ(st.track(1, 3).result, SeqResult::OK);
}

TEST(SequenceTracker, Gap) {
    SequenceTracker st;
    st.track(1, 0);
    st.track(1, 1);
    auto ev = st.track(1, 5);
    EXPECT_EQ(ev.result, SeqResult::GAP);
    EXPECT_EQ(ev.gap_size, 3u);
    EXPECT_EQ(ev.expected_seq, 2u);
    EXPECT_EQ(ev.actual_seq, 5u);
}

TEST(SequenceTracker, Reorder) {
    SequenceTracker st;
    st.track(1, 0);
    st.track(1, 1);
    st.track(1, 2);
    st.track(1, 5); // GAP (missing 3,4)
    auto ev = st.track(1, 3); // arrives late
    EXPECT_EQ(ev.result, SeqResult::REORDER);
}

TEST(SequenceTracker, Duplicate) {
    SequenceTracker st;
    st.track(1, 0);
    st.track(1, 1);
    st.track(1, 2);
    auto ev = st.track(1, 2); // exact duplicate
    EXPECT_EQ(ev.result, SeqResult::DUPLICATE);
}

TEST(SequenceTracker, MultipleSources) {
    SequenceTracker st;
    EXPECT_EQ(st.track(1, 0).result, SeqResult::FIRST);
    EXPECT_EQ(st.track(2, 0).result, SeqResult::FIRST);
    EXPECT_EQ(st.track(1, 1).result, SeqResult::OK);
    EXPECT_EQ(st.track(2, 1).result, SeqResult::OK);
    EXPECT_EQ(st.source_count(), 2u);
}

TEST(SequenceTracker, ResetSource) {
    SequenceTracker st;
    st.track(1, 0);
    st.track(1, 1);
    st.reset(1);
    EXPECT_EQ(st.source_count(), 0u);
    auto ev = st.track(1, 5); // should be FIRST again
    EXPECT_EQ(ev.result, SeqResult::FIRST);
}

TEST(SequenceTracker, LargeGap) {
    SequenceTracker st;
    st.track(1, 0);
    auto ev = st.track(1, 1000);
    EXPECT_EQ(ev.result, SeqResult::GAP);
    EXPECT_EQ(ev.gap_size, 999u);
}
