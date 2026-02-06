#pragma once
#include "common/types.h"
#include <string>
#include <ostream>
#include <mutex>

namespace nng {

class Logger {
public:
    void set_level(Severity level);
    Severity get_level() const;

    // Set output stream (default: std::cout). Caller owns the stream lifetime.
    void set_output(std::ostream& os);

    // Log a structured message.
    // Format: 2025-07-15T14:23:01.001Z [INFO ] [TRACKING  ] EVT_TRACK_NEW       detail...
    void log(Severity sev, EventCategory cat,
             const std::string& event_name,
             const std::string& detail);

    static Logger& instance();

    // Prevent copy
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger();

    mutable std::mutex mutex_;
    Severity level_ = Severity::INFO;
    std::ostream* out_ = nullptr; // set in constructor to &std::cout
};

// Helpers for converting enums to padded strings (used by logger and tests)
const char* severity_str(Severity s);
const char* category_str(EventCategory c);

} // namespace nng
