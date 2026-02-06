#pragma once
#include "sensor_sim/object_generator.h"
#include <string>

namespace nng {

// Load a ScenarioProfile from a simple JSON file.
// Throws std::runtime_error on file-not-found or parse error.
ScenarioProfile load_scenario(const std::string& json_path);

// Load from a JSON string (for testing without files).
ScenarioProfile load_scenario_from_string(const std::string& json_content);

} // namespace nng
