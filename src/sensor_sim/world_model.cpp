#include "sensor_sim/world_model.h"
#include <cmath>
#include <algorithm>

namespace nng {

static constexpr double DEG_TO_RAD = 3.14159265358979323846 / 180.0;
static constexpr double RAD_TO_DEG = 180.0 / 3.14159265358979323846;
static constexpr double MIN_RANGE_M = 50.0;

void WorldModel::add_object(WorldObject obj) {
    objects_.push_back(std::move(obj));
}

const std::vector<WorldObject>& WorldModel::tick(double dt, double current_time_s) {
    for (auto& obj : objects_) {
        double heading_rad = obj.heading_deg * DEG_TO_RAD;

        // Radial component: positive heading component changes range
        double radial_delta = obj.speed_mps * std::cos(heading_rad) * dt;
        obj.range_m += radial_delta;

        // Tangential component: changes azimuth
        if (obj.range_m > MIN_RANGE_M) {
            double tangential_delta = obj.speed_mps * std::sin(heading_rad) * dt;
            obj.azimuth_deg += tangential_delta / obj.range_m * RAD_TO_DEG;

            // Normalize azimuth to [0, 360)
            while (obj.azimuth_deg < 0.0) obj.azimuth_deg += 360.0;
            while (obj.azimuth_deg >= 360.0) obj.azimuth_deg -= 360.0;
        }
    }

    // Remove expired or too-close objects
    objects_.erase(
        std::remove_if(objects_.begin(), objects_.end(),
            [current_time_s](const WorldObject& obj) {
                if (obj.range_m < MIN_RANGE_M) return true;
                if (current_time_s > obj.spawn_time_s + obj.lifetime_s) return true;
                return false;
            }),
        objects_.end());

    return objects_;
}

std::size_t WorldModel::active_count() const {
    return objects_.size();
}

const std::vector<WorldObject>& WorldModel::objects() const {
    return objects_;
}

} // namespace nng
