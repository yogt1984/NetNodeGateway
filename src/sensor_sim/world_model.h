#pragma once
#include "sensor_sim/object_generator.h"
#include <vector>
#include <cstddef>

namespace nng {

class WorldModel {
public:
    void add_object(WorldObject obj);
    const std::vector<WorldObject>& tick(double dt, double current_time_s);
    std::size_t active_count() const;
    const std::vector<WorldObject>& objects() const;

private:
    std::vector<WorldObject> objects_;
};

} // namespace nng
