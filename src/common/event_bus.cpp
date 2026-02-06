#include "common/event_bus.h"
#include <algorithm>

namespace nng {

uint32_t EventBus::subscribe(EventCategory cat, Callback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t id = next_id_++;
    subs_.push_back({id, cat, false, std::move(cb)});
    return id;
}

uint32_t EventBus::subscribe_all(Callback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t id = next_id_++;
    subs_.push_back({id, EventCategory::TRACKING, true, std::move(cb)});
    return id;
}

void EventBus::unsubscribe(uint32_t sub_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    subs_.erase(
        std::remove_if(subs_.begin(), subs_.end(),
            [sub_id](const Subscription& s) { return s.id == sub_id; }),
        subs_.end());
}

void EventBus::publish(const EventRecord& event) {
    // Copy subscriber list under lock, then invoke without lock held
    // to avoid deadlock if a callback calls publish/subscribe.
    std::vector<Callback> to_call;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& s : subs_) {
            if (s.all_categories || s.category == event.category) {
                to_call.push_back(s.cb);
            }
        }
    }
    for (const auto& cb : to_call) {
        cb(event);
    }
}

} // namespace nng
