#include "sensor_sim/object_generator.h"
#include <cmath>

namespace nng {

ScenarioProfile profile_idle() {
    return {"idle", 0, 2,
            {TrackClass::BIRD, TrackClass::UNKNOWN},
            0.01, 1000, 15000, 5, 30, 0.0};
}

ScenarioProfile profile_patrol() {
    return {"patrol", 3, 8,
            {TrackClass::FIXED_WING, TrackClass::ROTARY_WING, TrackClass::UAV_SMALL},
            0.1, 5000, 30000, 50, 300, 0.3};
}

ScenarioProfile profile_raid() {
    return {"raid", 10, 30,
            {TrackClass::UAV_SMALL, TrackClass::MISSILE, TrackClass::ROCKET_ARTILLERY},
            1.0, 3000, 25000, 100, 600, 0.8};
}

ScenarioProfile profile_stress() {
    return {"stress", 50, 100,
            {TrackClass::FIXED_WING, TrackClass::ROTARY_WING, TrackClass::UAV_SMALL,
             TrackClass::UAV_LARGE, TrackClass::MISSILE, TrackClass::ROCKET_ARTILLERY,
             TrackClass::BIRD, TrackClass::DECOY, TrackClass::UNKNOWN},
            10.0, 1000, 40000, 10, 800, 0.5};
}

ObjectGenerator::ObjectGenerator(const ScenarioProfile& profile, uint32_t seed)
    : profile_(profile), rng_(seed) {}

WorldObject ObjectGenerator::make_object(double spawn_time_s) {
    WorldObject obj{};
    obj.id = next_id_++;
    obj.spawn_time_s = spawn_time_s;

    // Pick random classification from allowed types
    std::uniform_int_distribution<size_t> type_dist(0, profile_.allowed_types.size() - 1);
    obj.classification = profile_.allowed_types[type_dist(rng_)];

    // Lifetime: 10-120 seconds
    std::uniform_real_distribution<double> lifetime_dist(10.0, 120.0);
    obj.lifetime_s = lifetime_dist(rng_);

    // Position
    std::uniform_real_distribution<double> az_dist(0.0, 360.0);
    std::uniform_real_distribution<double> el_dist(0.5, 45.0);
    std::uniform_real_distribution<double> range_dist(profile_.min_range_m, profile_.max_range_m);
    obj.azimuth_deg = az_dist(rng_);
    obj.elevation_deg = el_dist(rng_);
    obj.range_m = range_dist(rng_);

    // Kinematics
    std::uniform_real_distribution<double> speed_dist(profile_.min_speed_mps, profile_.max_speed_mps);
    std::uniform_real_distribution<double> heading_dist(0.0, 360.0);
    obj.speed_mps = speed_dist(rng_);
    obj.heading_deg = heading_dist(rng_);

    // RCS based on classification
    double base_rcs;
    switch (obj.classification) {
        case TrackClass::FIXED_WING:       base_rcs = 10.0; break;
        case TrackClass::ROTARY_WING:      base_rcs = 5.0; break;
        case TrackClass::UAV_SMALL:        base_rcs = -5.0; break;
        case TrackClass::UAV_LARGE:        base_rcs = 3.0; break;
        case TrackClass::MISSILE:          base_rcs = -10.0; break;
        case TrackClass::ROCKET_ARTILLERY: base_rcs = -8.0; break;
        case TrackClass::BIRD:             base_rcs = -20.0; break;
        case TrackClass::DECOY:            base_rcs = 15.0; break;
        default:                           base_rcs = 0.0; break;
    }
    std::normal_distribution<double> rcs_noise(0.0, 2.0);
    obj.rcs_dbsm = base_rcs + rcs_noise(rng_);

    // Hostile
    std::uniform_real_distribution<double> hostile_dist(0.0, 1.0);
    obj.is_hostile = hostile_dist(rng_) < profile_.hostile_probability;

    // Measurement noise stddev (proportional to range, inversely to RCS)
    double rcs_linear = std::pow(10.0, obj.rcs_dbsm / 10.0);
    obj.noise_stddev = std::max(1.0, obj.range_m / 1000.0 / std::max(0.01, rcs_linear));

    return obj;
}

std::vector<WorldObject> ObjectGenerator::generate_initial() {
    std::uniform_int_distribution<int> count_dist(profile_.min_objects, profile_.max_objects);
    int count = count_dist(rng_);

    std::vector<WorldObject> objects;
    objects.reserve(count);
    for (int i = 0; i < count; ++i) {
        objects.push_back(make_object(0.0));
    }
    return objects;
}

std::optional<WorldObject> ObjectGenerator::maybe_spawn(double current_time_s) {
    if (profile_.spawn_rate_hz <= 0.0)
        return std::nullopt;

    double interval = 1.0 / profile_.spawn_rate_hz;
    if (current_time_s - last_spawn_time_ >= interval) {
        last_spawn_time_ = current_time_s;
        return make_object(current_time_s);
    }
    return std::nullopt;
}

} // namespace nng
