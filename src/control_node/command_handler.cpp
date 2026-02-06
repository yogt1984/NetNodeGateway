#include "control_node/command_handler.h"
#include <sstream>
#include <algorithm>

namespace nng {

CommandHandler::CommandHandler(StatsManager& stats, Logger& logger)
    : stats_(stats), logger_(logger) {}

std::string CommandHandler::handle(const std::string& command) {
    if (command.empty())
        return "ERR EMPTY_COMMAND";

    // Parse command: first word is the verb
    std::istringstream iss(command);
    std::string verb;
    iss >> verb;

    // Convert to uppercase for comparison
    std::transform(verb.begin(), verb.end(), verb.begin(), ::toupper);

    std::string rest;
    std::getline(iss, rest);
    // Trim leading whitespace
    auto pos = rest.find_first_not_of(" \t");
    if (pos != std::string::npos)
        rest = rest.substr(pos);
    else
        rest.clear();

    if (verb == "GET")
        return handle_get(rest);
    else if (verb == "SET")
        return handle_set(rest);

    return "ERR UNKNOWN_COMMAND";
}

std::string CommandHandler::handle_get(const std::string& args) {
    std::string what = args;
    std::transform(what.begin(), what.end(), what.begin(), ::toupper);

    if (what == "HEALTH") {
        auto health = stats_.get_health();
        switch (health) {
            case HealthState::OK:       return "HEALTH OK";
            case HealthState::DEGRADED: return "HEALTH DEGRADED";
            case HealthState::ERROR:    return "HEALTH ERROR";
        }
        return "HEALTH UNKNOWN";
    }

    if (what == "STATS") {
        auto g = stats_.get_global_stats();
        std::ostringstream oss;
        oss << "STATS\n";
        oss << "rx_total=" << g.rx_total << "\n";
        oss << "malformed_total=" << g.malformed_total << "\n";
        oss << "gap_total=" << g.gap_total << "\n";
        oss << "reorder_total=" << g.reorder_total << "\n";
        oss << "duplicate_total=" << g.duplicate_total << "\n";
        oss << "crc_fail_total=" << g.crc_fail_total;
        return oss.str();
    }

    return "ERR UNKNOWN_COMMAND";
}

std::string CommandHandler::handle_set(const std::string& args) {
    // Expect KEY=VALUE
    auto eq_pos = args.find('=');
    if (eq_pos == std::string::npos)
        return "ERR INVALID_SET_SYNTAX";

    std::string key = args.substr(0, eq_pos);
    std::string value = args.substr(eq_pos + 1);

    // Trim whitespace
    auto trim = [](std::string& s) {
        auto start = s.find_first_not_of(" \t");
        auto end = s.find_last_not_of(" \t");
        if (start == std::string::npos) { s.clear(); return; }
        s = s.substr(start, end - start + 1);
    };
    trim(key);
    trim(value);

    // Convert key to uppercase
    std::transform(key.begin(), key.end(), key.begin(), ::toupper);

    // Special handling for known keys
    if (key == "LOG_LEVEL") {
        std::string val_upper = value;
        std::transform(val_upper.begin(), val_upper.end(), val_upper.begin(), ::toupper);

        Severity level = Severity::INFO;
        if (val_upper == "DEBUG") level = Severity::DEBUG;
        else if (val_upper == "INFO") level = Severity::INFO;
        else if (val_upper == "WARN") level = Severity::WARN;
        else if (val_upper == "ALARM") level = Severity::ALARM;
        else if (val_upper == "ERROR") level = Severity::ERROR;
        else if (val_upper == "FATAL") level = Severity::FATAL;
        else return "ERR INVALID_LOG_LEVEL";

        logger_.set_level(level);
        config_[key] = val_upper;
        return "OK LOG_LEVEL=" + val_upper;
    }

    if (key == "CRC") {
        std::string val_upper = value;
        std::transform(val_upper.begin(), val_upper.end(), val_upper.begin(), ::toupper);

        if (val_upper == "ON") {
            crc_enabled_ = true;
            config_[key] = "ON";
            return "OK CRC=ON";
        } else if (val_upper == "OFF") {
            crc_enabled_ = false;
            config_[key] = "OFF";
            return "OK CRC=OFF";
        }
        return "ERR INVALID_CRC_VALUE";
    }

    // Generic key-value storage
    config_[key] = value;
    return "OK " + key + "=" + value;
}

std::string CommandHandler::get_config(const std::string& key) const {
    auto it = config_.find(key);
    if (it != config_.end())
        return it->second;
    return "";
}

} // namespace nng
