#include <gtest/gtest.h>
#include "EventEngine.h"
#include "devices/PowerMonitorDevice.h"
#include "devices/DeviceFactory.h"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

TEST(PowerMonitorTest, StartsAndEmitsTelemetry) {
    EventEngine engine;
    std::atomic<int> count{0};
    engine.subscribe([&](const Event& e){
        if (e.type == EventType::TELEMETRY) ++count;
    });

    PowerMonitorDevice dev("power-monitor-0", engine);
    dev.start();
    std::this_thread::sleep_for(600ms);   // wait for at least one 500ms tick
    dev.stop();

    EXPECT_GT(count.load(), 0);
}

TEST(PowerMonitorTest, TelemetryPayloadContainsCpuTemp) {
    EventEngine engine;
    std::string last_payload;
    engine.subscribe([&](const Event& e){
        if (e.type == EventType::TELEMETRY) last_payload = e.payload;
    });

    PowerMonitorDevice dev("power-monitor-0", engine);
    dev.start();
    std::this_thread::sleep_for(600ms);
    dev.stop();

    ASSERT_FALSE(last_payload.empty());
    EXPECT_NE(last_payload.find("cpu_temp_c"), std::string::npos);
    EXPECT_NE(last_payload.find("thermal_state"), std::string::npos);
    EXPECT_NE(last_payload.find("outlets"), std::string::npos);
}

TEST(PowerMonitorTest, OutletCountIs8) {
    EventEngine engine;
    PowerMonitorDevice dev("power-monitor-0", engine);
    dev.start();
    std::this_thread::sleep_for(600ms);

    auto outlets = dev.outlets();
    EXPECT_EQ(outlets.size(), 8u);
    dev.stop();
}

TEST(PowerMonitorTest, OutletWattsPositive) {
    EventEngine engine;
    PowerMonitorDevice dev("power-monitor-0", engine);
    dev.start();
    std::this_thread::sleep_for(600ms);

    auto outlets = dev.outlets();
    for (auto& o : outlets)
        EXPECT_GE(o.watts(), 0.0);
    dev.stop();
}

TEST(PowerMonitorTest, FactoryCreatesPowerMonitor) {
    EventEngine engine;
    EXPECT_NO_THROW({
        auto dev = DeviceFactory::create("power_monitor", "pdu-0", engine);
        EXPECT_NE(dev, nullptr);
    });
}
