#include <gtest/gtest.h>
#include "sensor_sim/object_generator.h"
#include <set>
#include <cmath>

using namespace nng;

TEST(ObjectGenerator, IdleInitialCount) {
    ObjectGenerator gen(profile_idle(), 42);
    auto objs = gen.generate_initial();
    EXPECT_GE(static_cast<int>(objs.size()), 0);
    EXPECT_LE(static_cast<int>(objs.size()), 2);
}

TEST(ObjectGenerator, PatrolInitialCount) {
    ObjectGenerator gen(profile_patrol(), 42);
    auto objs = gen.generate_initial();
    EXPECT_GE(static_cast<int>(objs.size()), 3);
    EXPECT_LE(static_cast<int>(objs.size()), 8);
}

TEST(ObjectGenerator, RaidInitialCount) {
    ObjectGenerator gen(profile_raid(), 42);
    auto objs = gen.generate_initial();
    EXPECT_GE(static_cast<int>(objs.size()), 10);
    EXPECT_LE(static_cast<int>(objs.size()), 30);
}

TEST(ObjectGenerator, StressInitialCount) {
    ObjectGenerator gen(profile_stress(), 42);
    auto objs = gen.generate_initial();
    EXPECT_GE(static_cast<int>(objs.size()), 50);
    EXPECT_LE(static_cast<int>(objs.size()), 100);
}

TEST(ObjectGenerator, AllowedTypesRespected) {
    auto profile = profile_patrol();
    std::set<TrackClass> allowed(profile.allowed_types.begin(), profile.allowed_types.end());
    ObjectGenerator gen(profile, 42);
    auto objs = gen.generate_initial();
    for (const auto& obj : objs) {
        EXPECT_TRUE(allowed.count(obj.classification))
            << "Object has disallowed classification: "
            << static_cast<int>(obj.classification);
    }
}

TEST(ObjectGenerator, RangeWithinBounds) {
    auto profile = profile_patrol();
    ObjectGenerator gen(profile, 42);
    auto objs = gen.generate_initial();
    for (const auto& obj : objs) {
        EXPECT_GE(obj.range_m, profile.min_range_m);
        EXPECT_LE(obj.range_m, profile.max_range_m);
    }
}

TEST(ObjectGenerator, SpeedWithinBounds) {
    auto profile = profile_raid();
    ObjectGenerator gen(profile, 42);
    auto objs = gen.generate_initial();
    for (const auto& obj : objs) {
        EXPECT_GE(obj.speed_mps, profile.min_speed_mps);
        EXPECT_LE(obj.speed_mps, profile.max_speed_mps);
    }
}

TEST(ObjectGenerator, AzimuthInRange) {
    ObjectGenerator gen(profile_stress(), 42);
    auto objs = gen.generate_initial();
    for (const auto& obj : objs) {
        EXPECT_GE(obj.azimuth_deg, 0.0);
        EXPECT_LT(obj.azimuth_deg, 360.0);
    }
}

TEST(ObjectGenerator, UniqueIds) {
    ObjectGenerator gen(profile_stress(), 42);
    auto objs = gen.generate_initial();
    std::set<uint32_t> ids;
    for (const auto& obj : objs) {
        EXPECT_TRUE(ids.insert(obj.id).second)
            << "Duplicate ID: " << obj.id;
    }
}

TEST(ObjectGenerator, DeterministicWithSameSeed) {
    auto profile = profile_patrol();

    ObjectGenerator gen1(profile, 77);
    auto objs1 = gen1.generate_initial();

    ObjectGenerator gen2(profile, 77);
    auto objs2 = gen2.generate_initial();

    ASSERT_EQ(objs1.size(), objs2.size());
    for (size_t i = 0; i < objs1.size(); ++i) {
        EXPECT_EQ(objs1[i].id, objs2[i].id);
        EXPECT_DOUBLE_EQ(objs1[i].range_m, objs2[i].range_m);
        EXPECT_DOUBLE_EQ(objs1[i].azimuth_deg, objs2[i].azimuth_deg);
        EXPECT_EQ(objs1[i].classification, objs2[i].classification);
    }
}

TEST(ObjectGenerator, DifferentSeeds) {
    auto profile = profile_patrol();
    ObjectGenerator gen1(profile, 1);
    ObjectGenerator gen2(profile, 2);
    auto objs1 = gen1.generate_initial();
    auto objs2 = gen2.generate_initial();
    // Very unlikely to be identical with different seeds
    bool all_same = (objs1.size() == objs2.size());
    if (all_same && !objs1.empty()) {
        all_same = (objs1[0].range_m == objs2[0].range_m);
    }
    EXPECT_FALSE(all_same) << "Different seeds should produce different objects";
}

TEST(ObjectGenerator, MaybeSpawnWithHighRate) {
    ScenarioProfile profile = profile_raid(); // spawn_rate_hz = 1.0
    ObjectGenerator gen(profile, 42);
    gen.generate_initial();

    int spawned = 0;
    for (int i = 0; i < 10; ++i) {
        double t = 1.0 + i * 1.1; // each step > 1s interval
        auto obj = gen.maybe_spawn(t);
        if (obj.has_value()) spawned++;
    }
    EXPECT_GT(spawned, 0) << "High spawn rate should produce objects";
}

TEST(ObjectGenerator, MaybeSpawnWithZeroRate) {
    ScenarioProfile profile = profile_idle();
    profile.spawn_rate_hz = 0.0;
    ObjectGenerator gen(profile, 42);

    for (int i = 0; i < 100; ++i) {
        auto obj = gen.maybe_spawn(static_cast<double>(i));
        EXPECT_FALSE(obj.has_value())
            << "Zero spawn rate should never spawn";
    }
}

TEST(ObjectGenerator, HostileProbabilityZero) {
    ScenarioProfile profile = profile_idle(); // hostile_probability = 0.0
    ObjectGenerator gen(profile, 42);
    // Force enough objects
    profile.min_objects = 10;
    profile.max_objects = 10;
    ObjectGenerator gen2(profile, 42);
    auto objs = gen2.generate_initial();
    for (const auto& obj : objs) {
        EXPECT_FALSE(obj.is_hostile)
            << "0% hostile probability should produce no hostile objects";
    }
}

TEST(ObjectGenerator, LifetimePositive) {
    ObjectGenerator gen(profile_stress(), 42);
    auto objs = gen.generate_initial();
    for (const auto& obj : objs) {
        EXPECT_GT(obj.lifetime_s, 0.0);
    }
}
