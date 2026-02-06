#pragma once
#include "common/types.h"
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <random>

namespace nng {

struct WorldObject {
    uint32_t   id;
    TrackClass classification;
    double     spawn_time_s;
    double     lifetime_s;
    double     azimuth_deg;
    double     elevation_deg;
    double     range_m;
    double     speed_mps;
    double     heading_deg;
    double     rcs_dbsm;
    bool       is_hostile;
    double     noise_stddev;
};

struct ScenarioProfile {
    std::string name;
    int         min_objects;
    int         max_objects;
    std::vector<TrackClass> allowed_types;
    double      spawn_rate_hz;
    double      min_range_m;
    double      max_range_m;
    double      min_speed_mps;
    double      max_speed_mps;
    double      hostile_probability;
};

ScenarioProfile profile_idle();
ScenarioProfile profile_patrol();
ScenarioProfile profile_raid();
ScenarioProfile profile_stress();

class ObjectGenerator {
public:
    explicit ObjectGenerator(const ScenarioProfile& profile, uint32_t seed = 42);

    std::vector<WorldObject> generate_initial();
    std::optional<WorldObject> maybe_spawn(double current_time_s);

    const ScenarioProfile& profile() const { return profile_; }

private:
    WorldObject make_object(double spawn_time_s);

    ScenarioProfile profile_;
    std::mt19937 rng_;
    uint32_t next_id_ = 1;
    double accumulated_time_ = 0.0;
    double last_spawn_time_ = 0.0;
};

} // namespace nng
