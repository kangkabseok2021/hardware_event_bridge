#include <gtest/gtest.h>
#include "devices/DeviceFactory.h"

TEST(DeviceFactoryTest, CreatesSerialDevice) {
    EventEngine engine;
    auto dev = DeviceFactory::create("serial", "serial-0", engine);
    EXPECT_NE(dev, nullptr);
    EXPECT_EQ(dev->id(), "serial-0");
}

TEST(DeviceFactoryTest, CreatesI2cDevice) {
    EventEngine engine;
    auto dev = DeviceFactory::create("i2c", "i2c-0", engine);
    EXPECT_NE(dev, nullptr);
    EXPECT_EQ(dev->id(), "i2c-0");
}

TEST(DeviceFactoryTest, UnknownTypeThrows) {
    EventEngine engine;
    EXPECT_THROW(DeviceFactory::create("unknown", "x", engine), std::invalid_argument);
}

TEST(DeviceFactoryTest, SerialDeviceEmitsOnStart) {
    EventEngine engine;
    std::atomic<int> count{0};
    engine.subscribe([&](const Event&){ ++count; });
    auto dev = DeviceFactory::create("serial", "s0", engine);
    dev->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    dev->stop();
    EXPECT_GT(count.load(), 0);
}
