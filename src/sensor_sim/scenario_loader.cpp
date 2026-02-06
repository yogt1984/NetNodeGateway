#include "sensor_sim/scenario_loader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace nng {
namespace {

// Minimal JSON-like parser for ScenarioProfile.
// Handles: { "key": value, ... } where value is string, number, or array of strings.
// NOT a general-purpose JSON parser. Sufficient for our scenario files.

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::string extract_string(const std::string& s) {
    auto first = s.find('"');
    auto last = s.rfind('"');
    if (first == std::string::npos || last == std::string::npos || first == last)
        throw std::runtime_error("Invalid JSON string: " + s);
    return s.substr(first + 1, last - first - 1);
}

double extract_number(const std::string& s) {
    std::string t = trim(s);
    // Remove trailing comma
    if (!t.empty() && t.back() == ',') t.pop_back();
    t = trim(t);
    try {
        return std::stod(t);
    } catch (...) {
        throw std::runtime_error("Invalid number: " + t);
    }
}

std::vector<std::string> extract_string_array(const std::string& s) {
    auto open = s.find('[');
    auto close = s.find(']');
    if (open == std::string::npos || close == std::string::npos)
        throw std::runtime_error("Invalid JSON array: " + s);

    std::string inner = s.substr(open + 1, close - open - 1);
    std::vector<std::string> result;
    std::stringstream ss(inner);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (item.size() >= 2 && item.front() == '"' && item.back() == '"') {
            result.push_back(item.substr(1, item.size() - 2));
        }
    }
    return result;
}

TrackClass string_to_track_class(const std::string& s) {
    if (s == "UNKNOWN") return TrackClass::UNKNOWN;
    if (s == "FIXED_WING") return TrackClass::FIXED_WING;
    if (s == "ROTARY_WING") return TrackClass::ROTARY_WING;
    if (s == "UAV_SMALL") return TrackClass::UAV_SMALL;
    if (s == "UAV_LARGE") return TrackClass::UAV_LARGE;
    if (s == "MISSILE") return TrackClass::MISSILE;
    if (s == "ROCKET_ARTILLERY") return TrackClass::ROCKET_ARTILLERY;
    if (s == "BIRD") return TrackClass::BIRD;
    if (s == "DECOY") return TrackClass::DECOY;
    throw std::runtime_error("Unknown TrackClass: " + s);
}

} // anonymous namespace

ScenarioProfile load_scenario_from_string(const std::string& json_content) {
    ScenarioProfile profile{};

    // Parse line by line looking for "key": value
    std::istringstream stream(json_content);
    std::string line;
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '{' || line[0] == '}') continue;

        auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key = extract_string(line.substr(0, colon));
        std::string value_part = trim(line.substr(colon + 1));

        if (key == "name") {
            profile.name = extract_string(value_part);
        } else if (key == "min_objects") {
            profile.min_objects = static_cast<int>(extract_number(value_part));
        } else if (key == "max_objects") {
            profile.max_objects = static_cast<int>(extract_number(value_part));
        } else if (key == "spawn_rate_hz") {
            profile.spawn_rate_hz = extract_number(value_part);
        } else if (key == "min_range_m") {
            profile.min_range_m = extract_number(value_part);
        } else if (key == "max_range_m") {
            profile.max_range_m = extract_number(value_part);
        } else if (key == "min_speed_mps") {
            profile.min_speed_mps = extract_number(value_part);
        } else if (key == "max_speed_mps") {
            profile.max_speed_mps = extract_number(value_part);
        } else if (key == "hostile_probability") {
            profile.hostile_probability = extract_number(value_part);
        } else if (key == "allowed_types") {
            // May span multiple lines; accumulate until ']'
            std::string array_str = value_part;
            while (array_str.find(']') == std::string::npos && std::getline(stream, line)) {
                array_str += " " + trim(line);
            }
            auto type_strs = extract_string_array(array_str);
            for (const auto& ts : type_strs) {
                profile.allowed_types.push_back(string_to_track_class(ts));
            }
        }
    }

    if (profile.name.empty())
        throw std::runtime_error("JSON missing 'name' field");
    if (profile.allowed_types.empty())
        throw std::runtime_error("JSON missing or empty 'allowed_types'");

    return profile;
}

ScenarioProfile load_scenario(const std::string& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open scenario file: " + json_path);

    std::ostringstream ss;
    ss << file.rdbuf();
    return load_scenario_from_string(ss.str());
}

} // namespace nng
