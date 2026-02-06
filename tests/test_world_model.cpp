#include <gtest/gtest.h>
#include "sensor_sim/world_model.h"
#include <cmath>

using namespace nng;

static WorldObject make_obj(uint32_t id, double range, double speed,
                             double heading, double lifetime, double spawn = 0.0) {
    WorldObject obj{};
    obj.id = id;
    obj.classification = TrackClass::FIXED_WING;
    obj.spawn_time_s = spawn;
    obj.lifetime_s = lifetime;
    obj.azimuth_deg = 90.0;
    obj.elevation_deg = 10.0;
    obj.range_m = range;
    obj.speed_mps = speed;
    obj.heading_deg = heading;
    obj.rcs_dbsm = 5.0;
    obj.is_hostile = false;
    obj.noise_stddev = 1.0;
    return obj;
}

TEST(WorldModel, TickUpdatesPosition) {
    WorldModel wm;
    wm.add_object(make_obj(1, 10000.0, 100.0, 0.0, 60.0));
    double initial_range = wm.objects()[0].range_m;

    wm.tick(1.0, 1.0);
    ASSERT_EQ(wm.active_count(), 1u);
    // heading=0 means cos(0)=1, so range increases by speed*dt=100
    EXPECT_NEAR(wm.objects()[0].range_m, initial_range + 100.0, 0.01);
}

TEST(WorldModel, ObjectExpiresByLifetime) {
    WorldModel wm;
    wm.add_object(make_obj(1, 10000.0, 0.0, 0.0, 5.0, 0.0));

    wm.tick(0.1, 4.0);
    EXPECT_EQ(wm.active_count(), 1u) << "Should still be alive at t=4";

    wm.tick(0.1, 6.0);
    EXPECT_EQ(wm.active_count(), 0u) << "Should be removed at t=6 (lifetime=5)";
}

TEST(WorldModel, ObjectRemovedAtMinRange) {
    WorldModel wm;
    // heading=180 means cos(180deg)=-1, range decreases
    wm.add_object(make_obj(1, 100.0, 200.0, 180.0, 60.0));

    wm.tick(1.0, 1.0);
    EXPECT_EQ(wm.active_count(), 0u) << "Should be removed when range < 50";
}

TEST(WorldModel, MultipleObjectsIndependent) {
    WorldModel wm;
    wm.add_object(make_obj(1, 10000.0, 50.0, 0.0, 60.0));
    wm.add_object(make_obj(2, 20000.0, 100.0, 90.0, 60.0));
    wm.add_object(make_obj(3, 5000.0, 200.0, 180.0, 60.0));

    wm.tick(1.0, 1.0);
    EXPECT_EQ(wm.active_count(), 3u);

    // Obj 1: range should increase (heading=0, cos=1)
    // Obj 3: range should decrease (heading=180, cos=-1)
    double r1 = 0, r3 = 0;
    for (const auto& obj : wm.objects()) {
        if (obj.id == 1) r1 = obj.range_m;
        if (obj.id == 3) r3 = obj.range_m;
    }
    EXPECT_GT(r1, 10000.0);
    EXPECT_LT(r3, 5000.0);
}

TEST(WorldModel, EmptyWorldTickNoCrash) {
    WorldModel wm;
    const auto& objs = wm.tick(1.0, 1.0);
    EXPECT_TRUE(objs.empty());
    EXPECT_EQ(wm.active_count(), 0u);
}

TEST(WorldModel, AzimuthNormalized) {
    WorldModel wm;
    // heading=90 means sin(90)=1, tangential motion increases azimuth
    wm.add_object(make_obj(1, 1000.0, 500.0, 90.0, 60.0));
    wm.tick(5.0, 5.0);
    ASSERT_EQ(wm.active_count(), 1u);
    double az = wm.objects()[0].azimuth_deg;
    EXPECT_GE(az, 0.0);
    EXPECT_LT(az, 360.0);
}

TEST(WorldModel, ManyObjectsSomeExpire) {
    WorldModel wm;
    for (int i = 0; i < 20; ++i) {
        // Even IDs: short lifetime (2s), odd IDs: long lifetime (100s)
        double lt = (i % 2 == 0) ? 2.0 : 100.0;
        wm.add_object(make_obj(i + 1, 10000.0, 10.0, 0.0, lt, 0.0));
    }
    EXPECT_EQ(wm.active_count(), 20u);

    wm.tick(0.1, 3.0);
    // 10 even-ID objects should be expired (lifetime=2, t=3)
    EXPECT_EQ(wm.active_count(), 10u);
}
