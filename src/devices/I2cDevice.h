#pragma once
#include "IDevice.h"
#include <atomic>
#include <chrono>
#include <random>
#include <thread>

// Simulates an I2C sensor (e.g. BME280 on /dev/i2c-1, addr 0x76).
// On real hardware: uses ioctl(fd, I2C_SLAVE, addr) + read()/write()
// from within the epoll loop.
class I2cDevice : public IDevice {
public:
    explicit I2cDevice(DeviceId id, EventEngine& engine, uint8_t addr = 0x76)
        : IDevice(std::move(id), engine), addr_(addr) {}

    void start() override {
        running_ = true;
        thread_ = std::thread([this] {
            std::mt19937 rng(addr_);
            std::uniform_real_distribution<double> hum_dist(40.0, 60.0);
            std::uniform_real_distribution<double> pres_dist(1013.0, 1015.0);

            emit(EventType::STATE_CHANGE,
                "{\"status\":\"connected\",\"addr\":\"0x" +
                std::to_string(addr_) + "\"}");

            while (running_) {
                emit(EventType::TELEMETRY,
                    "{\"humidity_pct\":" + std::to_string(hum_dist(rng)) +
                    ",\"pressure_hpa\":" + std::to_string(pres_dist(rng)) + "}");
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        });
    }

    void stop() override {
        running_ = false;
        if (thread_.joinable()) thread_.join();
    }

private:
    std::atomic<bool> running_{false};
    std::thread       thread_;
    uint8_t           addr_;
};
