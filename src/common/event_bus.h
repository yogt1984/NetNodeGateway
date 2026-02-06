#pragma once
#include "common/types.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <mutex>

namespace nng {

struct EventRecord {
    EventId       id;
    EventCategory category;
    Severity      severity;
    uint64_t      timestamp_ns;
    std::string   detail;
};

class EventBus {
public:
    using Callback = std::function<void(const EventRecord&)>;

    // Subscribe to a specific category. Returns subscription ID.
    uint32_t subscribe(EventCategory cat, Callback cb);

    // Subscribe to all events. Returns subscription ID.
    uint32_t subscribe_all(Callback cb);

    // Unsubscribe by ID.
    void unsubscribe(uint32_t sub_id);

    // Publish an event (calls matching subscribers synchronously).
    void publish(const EventRecord& event);

private:
    struct Subscription {
        uint32_t      id;
        EventCategory category;
        bool          all_categories;
        Callback      cb;
    };

    std::mutex mutex_;
    std::vector<Subscription> subs_;
    uint32_t next_id_ = 1;
};

} // namespace nng
