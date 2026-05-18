#include <gtest/gtest.h>
#include "DeviceManager.h"

TEST(DeviceManagerTest, RegisterAndGet) {
    DeviceManager mgr;
    mgr.registerDevice("serial-0", "serial");
    auto dev = mgr.getDevice("serial-0");
    ASSERT_TRUE(dev.has_value());
    EXPECT_EQ(dev->device_id,   "serial-0");
    EXPECT_EQ(dev->device_type, "serial");
    EXPECT_EQ(dev->status,      DeviceStatus::IDLE);
}

TEST(DeviceManagerTest, UnknownDeviceReturnsNullopt) {
    DeviceManager mgr;
    EXPECT_FALSE(mgr.getDevice("no-such-device").has_value());
}

TEST(DeviceManagerTest, TelemetryEventUpdatesPayload) {
    DeviceManager mgr;
    mgr.registerDevice("i2c-0", "i2c");
    mgr.onEvent({"i2c-0", EventType::TELEMETRY, R"({"temp":25})"});
    auto dev = mgr.getDevice("i2c-0");
    EXPECT_EQ(dev->last_payload, R"({"temp":25})");
    EXPECT_EQ(dev->status, DeviceStatus::CONNECTED);
}

TEST(DeviceManagerTest, ErrorEventSetsErrorStatus) {
    DeviceManager mgr;
    mgr.registerDevice("serial-0", "serial");
    mgr.onEvent({"serial-0", EventType::ERROR, "timeout"});
    EXPECT_EQ(mgr.getDevice("serial-0")->status, DeviceStatus::ERROR);
}

TEST(DeviceManagerTest, GetAllDevices) {
    DeviceManager mgr;
    mgr.registerDevice("a", "serial");
    mgr.registerDevice("b", "i2c");
    EXPECT_EQ(mgr.getAllDevices().size(), 2u);
    EXPECT_EQ(mgr.deviceCount(), 2u);
}

TEST(DeviceManagerTest, StateChangeConnects) {
    DeviceManager mgr;
    mgr.registerDevice("dev", "serial");
    mgr.onEvent({"dev", EventType::STATE_CHANGE, R"({"status":"connected"})"});
    EXPECT_EQ(mgr.getDevice("dev")->status, DeviceStatus::CONNECTED);
}
