#include <gtest/gtest.h>
#include "sensor_sim/fault_injector.h"
#include <numeric>
#include <cmath>

using namespace nng;

static std::vector<std::vector<uint8_t>> make_frames(int count) {
    std::vector<std::vector<uint8_t>> frames;
    for (int i = 0; i < count; ++i) {
        // Each frame: 4 bytes containing its index
        std::vector<uint8_t> f(4);
        f[0] = static_cast<uint8_t>(i & 0xFF);
        f[1] = static_cast<uint8_t>((i >> 8) & 0xFF);
        f[2] = 0xAA;
        f[3] = 0xBB;
        frames.push_back(f);
    }
    return frames;
}

TEST(FaultInjector, NoFaultsIdenticalOutput) {
    FaultConfig cfg{};
    FaultInjector fi(cfg, 42);
    auto frames = make_frames(100);
    auto original = frames;
    fi.apply(frames);
    EXPECT_EQ(frames.size(), original.size());
    for (size_t i = 0; i < frames.size(); ++i) {
        EXPECT_EQ(frames[i], original[i]) << "Frame " << i << " changed with no faults";
    }
    auto stats = fi.last_stats();
    EXPECT_EQ(stats.dropped, 0u);
    EXPECT_EQ(stats.reordered, 0u);
    EXPECT_EQ(stats.duplicated, 0u);
    EXPECT_EQ(stats.corrupted, 0u);
}

TEST(FaultInjector, Loss100PercentDropsAll) {
    FaultConfig cfg{};
    cfg.loss_pct = 100.0;
    FaultInjector fi(cfg, 42);
    auto frames = make_frames(50);
    fi.apply(frames);
    EXPECT_EQ(frames.size(), 0u);
    EXPECT_EQ(fi.last_stats().dropped, 50u);
}

TEST(FaultInjector, LossZeroPercentDropsNone) {
    FaultConfig cfg{};
    cfg.loss_pct = 0.0;
    FaultInjector fi(cfg, 42);
    auto frames = make_frames(50);
    fi.apply(frames);
    EXPECT_EQ(frames.size(), 50u);
}

TEST(FaultInjector, Loss50PercentApproximate) {
    FaultConfig cfg{};
    cfg.loss_pct = 50.0;
    FaultInjector fi(cfg, 42);
    auto frames = make_frames(1000);
    fi.apply(frames);
    // Should drop roughly 500 +/- 10%
    int remaining = static_cast<int>(frames.size());
    EXPECT_GT(remaining, 350) << "Too many dropped: " << (1000 - remaining);
    EXPECT_LT(remaining, 650) << "Too few dropped: " << (1000 - remaining);
}

TEST(FaultInjector, ReorderChangesOrder) {
    FaultConfig cfg{};
    cfg.reorder_pct = 100.0;
    FaultInjector fi(cfg, 42);
    auto frames = make_frames(100);
    auto original = frames;
    fi.apply(frames);
    EXPECT_EQ(frames.size(), original.size());

    // At least some frames should be in different positions
    int changed = 0;
    for (size_t i = 0; i < frames.size(); ++i) {
        if (frames[i] != original[i]) changed++;
    }
    EXPECT_GT(changed, 0) << "100% reorder should change at least some positions";
    EXPECT_GT(fi.last_stats().reordered, 0u);
}

TEST(FaultInjector, DuplicateIncreasesCount) {
    FaultConfig cfg{};
    cfg.duplicate_pct = 50.0;
    FaultInjector fi(cfg, 42);
    auto frames = make_frames(100);
    fi.apply(frames);
    EXPECT_GT(frames.size(), 100u) << "Duplicates should increase frame count";
    EXPECT_GT(fi.last_stats().duplicated, 0u);
}

TEST(FaultInjector, CorruptFlipsByte) {
    FaultConfig cfg{};
    cfg.corrupt_pct = 100.0;
    FaultInjector fi(cfg, 42);
    auto frames = make_frames(50);
    auto original = frames;
    fi.apply(frames);
    EXPECT_EQ(frames.size(), original.size());

    int corrupted = 0;
    for (size_t i = 0; i < frames.size(); ++i) {
        if (frames[i] != original[i]) corrupted++;
    }
    EXPECT_EQ(corrupted, 50) << "100% corrupt should corrupt all frames";
    EXPECT_EQ(fi.last_stats().corrupted, 50u);
}

TEST(FaultInjector, DeterministicWithSameSeed) {
    FaultConfig cfg{};
    cfg.loss_pct = 30.0;
    cfg.reorder_pct = 20.0;
    cfg.duplicate_pct = 10.0;
    cfg.corrupt_pct = 5.0;

    auto frames1 = make_frames(200);
    auto frames2 = make_frames(200);

    FaultInjector fi1(cfg, 77);
    fi1.apply(frames1);

    FaultInjector fi2(cfg, 77);
    fi2.apply(frames2);

    EXPECT_EQ(frames1.size(), frames2.size());
    for (size_t i = 0; i < frames1.size(); ++i) {
        EXPECT_EQ(frames1[i], frames2[i]) << "Determinism broken at frame " << i;
    }
}

TEST(FaultInjector, EmptyInputNoCrash) {
    FaultConfig cfg{};
    cfg.loss_pct = 50.0;
    cfg.reorder_pct = 50.0;
    FaultInjector fi(cfg, 42);
    std::vector<std::vector<uint8_t>> empty;
    EXPECT_NO_THROW(fi.apply(empty));
    EXPECT_TRUE(empty.empty());
}

TEST(FaultInjector, SingleFrameWithAllFaults) {
    FaultConfig cfg{};
    cfg.loss_pct = 50.0;
    cfg.reorder_pct = 50.0;
    cfg.duplicate_pct = 50.0;
    cfg.corrupt_pct = 50.0;
    FaultInjector fi(cfg, 42);
    auto frames = make_frames(1);
    EXPECT_NO_THROW(fi.apply(frames));
    // Could be 0, 1, or 2 frames after all faults
}

TEST(FaultInjector, StatsResetEachApply) {
    FaultConfig cfg{};
    cfg.loss_pct = 100.0;
    FaultInjector fi(cfg, 42);

    auto frames1 = make_frames(10);
    fi.apply(frames1);
    EXPECT_EQ(fi.last_stats().dropped, 10u);

    auto frames2 = make_frames(5);
    fi.apply(frames2);
    EXPECT_EQ(fi.last_stats().dropped, 5u) << "Stats should reset between apply calls";
}
