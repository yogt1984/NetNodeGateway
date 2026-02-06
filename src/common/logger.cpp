#include "common/logger.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <ctime>
#include <cstring>

namespace nng {

const char* severity_str(Severity s) {
    switch (s) {
        case Severity::DEBUG: return "DEBUG";
        case Severity::INFO:  return "INFO ";
        case Severity::WARN:  return "WARN ";
        case Severity::ALARM: return "ALARM";
        case Severity::ERROR: return "ERROR";
        case Severity::FATAL: return "FATAL";
    }
    return "?????";
}

const char* category_str(EventCategory c) {
    switch (c) {
        case EventCategory::TRACKING:   return "TRACKING  ";
        case EventCategory::THREAT:     return "THREAT    ";
        case EventCategory::IFF:        return "IFF       ";
        case EventCategory::ENGAGEMENT: return "ENGAGEMENT";
        case EventCategory::NETWORK:    return "NETWORK   ";
        case EventCategory::HEALTH:     return "HEALTH    ";
        case EventCategory::CONTROL:    return "CONTROL   ";
    }
    return "??????????";
}

Logger::Logger() : out_(&std::cout) {}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::set_level(Severity level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

Severity Logger::get_level() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return level_;
}

void Logger::set_output(std::ostream& os) {
    std::lock_guard<std::mutex> lock(mutex_);
    out_ = &os;
}

void Logger::log(Severity sev, EventCategory cat,
                 const std::string& event_name,
                 const std::string& detail) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Severity filter
    if (static_cast<uint8_t>(sev) < static_cast<uint8_t>(level_))
        return;

    if (!out_)
        return;

    // Timestamp: ISO 8601 with milliseconds
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm utc{};
    gmtime_r(&time_t_now, &utc);

    // Pad event_name to 20 chars
    char evt_buf[21];
    std::memset(evt_buf, ' ', 20);
    evt_buf[20] = '\0';
    std::size_t name_len = event_name.size();
    if (name_len > 20) name_len = 20;
    std::memcpy(evt_buf, event_name.data(), name_len);

    *out_ << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S")
          << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z'
          << " [" << severity_str(sev) << "] "
          << "[" << category_str(cat) << "] "
          << evt_buf
          << detail << '\n';
}

} // namespace nng
