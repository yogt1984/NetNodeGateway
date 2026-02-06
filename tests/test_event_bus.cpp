#include <gtest/gtest.h>
#include "common/event_bus.h"
#include <atomic>
#include <thread>
#include <vector>

using namespace nng;

static EventRecord make_event(EventCategory cat, EventId id, const std::string& detail = "") {
    return EventRecord{id, cat, Severity::INFO, 0, detail};
}

TEST(EventBus, SubscribeCategoryReceivesMatchingEvents) {
    EventBus bus;
    int count = 0;
    bus.subscribe(EventCategory::TRACKING, [&](const EventRecord& e) {
        count++;
        EXPECT_EQ(e.category, EventCategory::TRACKING);
    });
    bus.publish(make_event(EventCategory::TRACKING, EventId::EVT_TRACK_NEW));
    bus.publish(make_event(EventCategory::TRACKING, EventId::EVT_TRACK_UPDATE));
    EXPECT_EQ(count, 2);
}

TEST(EventBus, SubscribeCategoryIgnoresNonMatching) {
    EventBus bus;
    int count = 0;
    bus.subscribe(EventCategory::TRACKING, [&](const EventRecord&) { count++; });
    bus.publish(make_event(EventCategory::NETWORK, EventId::EVT_SEQ_GAP));
    bus.publish(make_event(EventCategory::HEALTH, EventId::EVT_HEARTBEAT_OK));
    EXPECT_EQ(count, 0) << "TRACKING subscriber should not fire for NETWORK/HEALTH events";
}

TEST(EventBus, SubscribeAllReceivesEverything) {
    EventBus bus;
    int count = 0;
    bus.subscribe_all([&](const EventRecord&) { count++; });
    bus.publish(make_event(EventCategory::TRACKING, EventId::EVT_TRACK_NEW));
    bus.publish(make_event(EventCategory::NETWORK, EventId::EVT_SEQ_GAP));
    bus.publish(make_event(EventCategory::HEALTH, EventId::EVT_HEARTBEAT_OK));
    bus.publish(make_event(EventCategory::IFF, EventId::EVT_IFF_FOE));
    EXPECT_EQ(count, 4);
}

TEST(EventBus, UnsubscribeStopsDelivery) {
    EventBus bus;
    int count = 0;
    auto id = bus.subscribe(EventCategory::TRACKING, [&](const EventRecord&) { count++; });
    bus.publish(make_event(EventCategory::TRACKING, EventId::EVT_TRACK_NEW));
    EXPECT_EQ(count, 1);

    bus.unsubscribe(id);
    bus.publish(make_event(EventCategory::TRACKING, EventId::EVT_TRACK_UPDATE));
    EXPECT_EQ(count, 1) << "Should not increment after unsubscribe";
}

TEST(EventBus, MultipleSubscribersSameCategory) {
    EventBus bus;
    int count_a = 0, count_b = 0;
    bus.subscribe(EventCategory::NETWORK, [&](const EventRecord&) { count_a++; });
    bus.subscribe(EventCategory::NETWORK, [&](const EventRecord&) { count_b++; });
    bus.publish(make_event(EventCategory::NETWORK, EventId::EVT_CRC_FAIL));
    EXPECT_EQ(count_a, 1);
    EXPECT_EQ(count_b, 1);
}

TEST(EventBus, PublishWithNoSubscribersDoesNotCrash) {
    EventBus bus;
    // Should not throw or crash
    EXPECT_NO_THROW(
        bus.publish(make_event(EventCategory::ENGAGEMENT, EventId::EVT_ENGAGE_START)));
}

TEST(EventBus, UnsubscribeInvalidIdDoesNotCrash) {
    EventBus bus;
    EXPECT_NO_THROW(bus.unsubscribe(999));
}

TEST(EventBus, EventRecordFieldsDeliveredCorrectly) {
    EventBus bus;
    EventRecord received{};
    bus.subscribe(EventCategory::THREAT, [&](const EventRecord& e) { received = e; });

    EventRecord sent{};
    sent.id = EventId::EVT_THREAT_CRITICAL;
    sent.category = EventCategory::THREAT;
    sent.severity = Severity::ALARM;
    sent.timestamp_ns = 123456789;
    sent.detail = "track_id=1042 threat=CRITICAL";

    bus.publish(sent);

    EXPECT_EQ(received.id, EventId::EVT_THREAT_CRITICAL);
    EXPECT_EQ(received.category, EventCategory::THREAT);
    EXPECT_EQ(received.severity, Severity::ALARM);
    EXPECT_EQ(received.timestamp_ns, 123456789u);
    EXPECT_EQ(received.detail, "track_id=1042 threat=CRITICAL");
}

TEST(EventBus, MixedCategoryAndAllSubscribers) {
    EventBus bus;
    int cat_count = 0, all_count = 0;
    bus.subscribe(EventCategory::TRACKING, [&](const EventRecord&) { cat_count++; });
    bus.subscribe_all([&](const EventRecord&) { all_count++; });

    bus.publish(make_event(EventCategory::TRACKING, EventId::EVT_TRACK_NEW));
    bus.publish(make_event(EventCategory::NETWORK, EventId::EVT_SEQ_GAP));

    EXPECT_EQ(cat_count, 1) << "Category sub should only fire for TRACKING";
    EXPECT_EQ(all_count, 2) << "All-sub should fire for both";
}

TEST(EventBus, UnsubscribeOneKeepsOthers) {
    EventBus bus;
    int count_a = 0, count_b = 0;
    auto id_a = bus.subscribe(EventCategory::NETWORK, [&](const EventRecord&) { count_a++; });
    bus.subscribe(EventCategory::NETWORK, [&](const EventRecord&) { count_b++; });

    bus.unsubscribe(id_a);
    bus.publish(make_event(EventCategory::NETWORK, EventId::EVT_SOURCE_ONLINE));

    EXPECT_EQ(count_a, 0) << "Unsubscribed callback should not fire";
    EXPECT_EQ(count_b, 1) << "Remaining callback should still fire";
}

TEST(EventBus, ThreadSafePublish) {
    EventBus bus;
    std::atomic<int> count{0};
    bus.subscribe_all([&](const EventRecord&) { count++; });

    constexpr int threads = 4;
    constexpr int per_thread = 50;
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&]() {
            for (int i = 0; i < per_thread; ++i) {
                bus.publish(make_event(EventCategory::TRACKING, EventId::EVT_TRACK_UPDATE));
            }
        });
    }
    for (auto& th : pool) th.join();

    EXPECT_EQ(count.load(), threads * per_thread);
}
