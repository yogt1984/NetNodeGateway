#pragma once
#include "gateway/stats_manager.h"
#include "common/logger.h"
#include <string>
#include <unordered_map>
#include <functional>

namespace nng {

class CommandHandler {
public:
    explicit CommandHandler(StatsManager& stats, Logger& logger);

    // Process a command string, return a response string
    std::string handle(const std::string& command);

    // Get current config value (for testing)
    std::string get_config(const std::string& key) const;

    // Check if CRC is enabled
    bool crc_enabled() const { return crc_enabled_; }

private:
    std::string handle_get(const std::string& args);
    std::string handle_set(const std::string& args);

    StatsManager& stats_;
    Logger& logger_;
    std::unordered_map<std::string, std::string> config_;
    bool crc_enabled_ = true;
};

} // namespace nng
