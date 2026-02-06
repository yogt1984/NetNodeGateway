#include <gtest/gtest.h>
#include "sensor_sim/scenario_loader.h"
#include <fstream>
#include <cstdio>

using namespace nng;

TEST(ScenarioLoader, LoadPatrolFromFile) {
    auto profile = load_scenario("../scenarios/patrol.json");
    EXPECT_EQ(profile.name, "patrol");
    EXPECT_EQ(profile.min_objects, 3);
    EXPECT_EQ(profile.max_objects, 8);
    EXPECT_DOUBLE_EQ(profile.spawn_rate_hz, 0.1);
    EXPECT_DOUBLE_EQ(profile.min_range_m, 5000.0);
    EXPECT_DOUBLE_EQ(profile.max_range_m, 30000.0);
    EXPECT_DOUBLE_EQ(profile.min_speed_mps, 50.0);
    EXPECT_DOUBLE_EQ(profile.max_speed_mps, 300.0);
    EXPECT_DOUBLE_EQ(profile.hostile_probability, 0.3);
    ASSERT_EQ(profile.allowed_types.size(), 3u);
    EXPECT_EQ(profile.allowed_types[0], TrackClass::FIXED_WING);
    EXPECT_EQ(profile.allowed_types[1], TrackClass::ROTARY_WING);
    EXPECT_EQ(profile.allowed_types[2], TrackClass::UAV_SMALL);
}

TEST(ScenarioLoader, LoadRaidFromFile) {
    auto profile = load_scenario("../scenarios/raid.json");
    EXPECT_EQ(profile.name, "raid");
    EXPECT_EQ(profile.min_objects, 10);
    EXPECT_EQ(profile.max_objects, 30);
    EXPECT_DOUBLE_EQ(profile.hostile_probability, 0.8);
    ASSERT_EQ(profile.allowed_types.size(), 3u);
    EXPECT_EQ(profile.allowed_types[0], TrackClass::UAV_SMALL);
    EXPECT_EQ(profile.allowed_types[1], TrackClass::MISSILE);
    EXPECT_EQ(profile.allowed_types[2], TrackClass::ROCKET_ARTILLERY);
}

TEST(ScenarioLoader, LoadIdleFromFile) {
    auto profile = load_scenario("../scenarios/idle.json");
    EXPECT_EQ(profile.name, "idle");
    EXPECT_EQ(profile.min_objects, 0);
    EXPECT_DOUBLE_EQ(profile.hostile_probability, 0.0);
}

TEST(ScenarioLoader, LoadStressFromFile) {
    auto profile = load_scenario("../scenarios/stress.json");
    EXPECT_EQ(profile.name, "stress");
    EXPECT_EQ(profile.min_objects, 50);
    EXPECT_EQ(profile.max_objects, 100);
    ASSERT_EQ(profile.allowed_types.size(), 9u);
}

TEST(ScenarioLoader, NonexistentFileThrows) {
    EXPECT_THROW(load_scenario("/nonexistent/path/bogus.json"), std::runtime_error);
}

TEST(ScenarioLoader, MalformedJsonThrows) {
    std::string bad_json = "{ this is not valid json at all";
    EXPECT_THROW(load_scenario_from_string(bad_json), std::runtime_error);
}

TEST(ScenarioLoader, MissingNameThrows) {
    std::string json = R"({
        "min_objects": 1,
        "max_objects": 2,
        "allowed_types": ["BIRD"]
    })";
    EXPECT_THROW(load_scenario_from_string(json), std::runtime_error);
}

TEST(ScenarioLoader, MissingAllowedTypesThrows) {
    std::string json = R"({
        "name": "test"
    })";
    EXPECT_THROW(load_scenario_from_string(json), std::runtime_error);
}

TEST(ScenarioLoader, LoadFromString) {
    std::string json = R"({
        "name": "custom",
        "min_objects": 5,
        "max_objects": 10,
        "allowed_types": ["MISSILE", "DECOY"],
        "spawn_rate_hz": 2.5,
        "min_range_m": 2000,
        "max_range_m": 15000,
        "min_speed_mps": 100,
        "max_speed_mps": 500,
        "hostile_probability": 0.9
    })";
    auto profile = load_scenario_from_string(json);
    EXPECT_EQ(profile.name, "custom");
    EXPECT_EQ(profile.min_objects, 5);
    EXPECT_EQ(profile.max_objects, 10);
    ASSERT_EQ(profile.allowed_types.size(), 2u);
    EXPECT_EQ(profile.allowed_types[0], TrackClass::MISSILE);
    EXPECT_EQ(profile.allowed_types[1], TrackClass::DECOY);
    EXPECT_DOUBLE_EQ(profile.hostile_probability, 0.9);
}

TEST(ScenarioLoader, UnknownTrackClassThrows) {
    std::string json = R"({
        "name": "bad",
        "min_objects": 1,
        "max_objects": 1,
        "allowed_types": ["SPACESHIP"]
    })";
    EXPECT_THROW(load_scenario_from_string(json), std::runtime_error);
}
