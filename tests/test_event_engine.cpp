#include <gtest/gtest.h>
#include "EventEngine.h"
#include <atomic>
#include <thread>

TEST(EventEngineTest, SubscribeAndEmit) {
    EventEngine engine;
    std::atomic<int> count{0};
    engine.subscribe([&](const Event&){ ++count; });
    engine.emit({"dev1", EventType::TELEMETRY, "{}"});
    EXPECT_EQ(count.load(), 1);
}

TEST(EventEngineTest, MultipleListeners) {
    EventEngine engine;
    std::atomic<int> a{0}, b{0};
    engine.subscribe([&](const Event&){ ++a; });
    engine.subscribe([&](const Event&){ ++b; });
    engine.emit({"dev1", EventType::HEARTBEAT, ""});
    EXPECT_EQ(a.load(), 1);
    EXPECT_EQ(b.load(), 1);
}

TEST(EventEngineTest, ListenerCount) {
    EventEngine engine;
    EXPECT_EQ(engine.listenerCount(), 0u);
    engine.subscribe([](const Event&){});
    engine.subscribe([](const Event&){});
    EXPECT_EQ(engine.listenerCount(), 2u);
}

TEST(EventEngineTest, EventPayloadPreserved) {
    EventEngine engine;
    std::string received;
    engine.subscribe([&](const Event& e){ received = e.payload; });
    engine.emit({"dev1", EventType::TELEMETRY, R"({"temp":42})"});
    EXPECT_EQ(received, R"({"temp":42})");
}

TEST(EventEngineTest, ConcurrentEmit) {
    EventEngine engine;
    std::atomic<int> total{0};
    engine.subscribe([&](const Event&){ ++total; });
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i)
        threads.emplace_back([&]{ engine.emit({"dev1", EventType::TELEMETRY, ""}); });
    for (auto& t : threads) t.join();
    EXPECT_EQ(total.load(), 10);
}
