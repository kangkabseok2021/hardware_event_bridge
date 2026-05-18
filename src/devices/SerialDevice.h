#pragma once
#include "IDevice.h"
#include <atomic>
#include <chrono>
#include <random>
#include <thread>

// Simulates a serial sensor (e.g. UART temperature probe on /dev/ttyACM0).
// On real hardware: opens the port via termios2, reads NMEA-like frames
// from the epoll loop, and parses them here.
class SerialDevice : public IDevice {
public:
    using IDevice::IDevice;

    void start() override {
        running_ = true;
        thread_ = std::thread([this] {
            std::mt19937 rng(std::hash<std::string>{}(id_));
            std::uniform_real_distribution<double> temp_dist(20.0, 25.0);
            std::uniform_real_distribution<double> volt_dist(4.9, 5.1);

            emit(EventType::STATE_CHANGE, R"({"status":"connected","port":"/dev/ttyACM0"})");

            while (running_) {
                double temp  = temp_dist(rng);
                double volts = volt_dist(rng);
                emit(EventType::TELEMETRY,
                    "{\"temp_c\":" + std::to_string(temp) +
                    ",\"voltage\":" + std::to_string(volts) + "}");
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        });
    }

    void stop() override {
        running_ = false;
        if (thread_.joinable()) thread_.join();
        emit(EventType::STATE_CHANGE, R"({"status":"disconnected"})");
    }

private:
    std::atomic<bool> running_{false};
    std::thread       thread_;
};
