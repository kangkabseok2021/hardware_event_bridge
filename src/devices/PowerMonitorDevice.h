#pragma once
#include "IDevice.h"
#include "ThermalFsm.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <random>
#include <sstream>
#include <thread>

static constexpr int PDU_OUTLETS = 8;

// Reads real board telemetry via Linux kernel interfaces:
//   /sys/class/thermal/thermal_zone0/temp  → CPU temperature (m°C)
//   /proc/meminfo                           → MemTotal / MemAvailable
// Simulates 8 PDU outlets: voltage (120 V nominal) + current (Gaussian noise
// around a sinusoidal load profile). Emits TELEMETRY events every 500 ms.
//
// On non-Linux platforms (CI / macOS dev) returns synthetic values so the
// same code compiles and tests without hardware.
class PowerMonitorDevice : public IDevice {
public:
    struct Outlet {
        int    id;
        double voltage_v;   // nominal 120 V ± noise
        double current_a;   // load profile + Gaussian noise
        double watts() const { return voltage_v * current_a; }
        bool   active{true};
    };

    using IDevice::IDevice;

    void start() override {
        running_ = true;
        thread_  = std::thread([this]{ run(); });
        emit(EventType::STATE_CHANGE, R"({"status":"connected","type":"power_monitor"})");
    }

    void stop() override {
        running_ = false;
        if (thread_.joinable()) thread_.join();
    }

    // Snapshot for REST API — lock-free copy
    std::vector<Outlet> outlets() const {
        std::lock_guard<std::mutex> lk(mu_);
        return outlets_;
    }
    double cpuTempC()    const { return cpu_temp_c_.load(); }
    int    memUsedPct()  const { return mem_used_pct_.load(); }
    ThermalState thermalState() const { return thermal_state_.load(); }

private:
    void run() {
        std::mt19937 rng(42);
        std::normal_distribution<double> v_noise(0.0, 0.5);   // voltage noise σ=0.5V
        std::normal_distribution<double> i_noise(0.0, 0.05);  // current noise σ=0.05A

        // Initialise outlets
        {
            std::lock_guard<std::mutex> lk(mu_);
            outlets_.resize(PDU_OUTLETS);
            for (int i = 0; i < PDU_OUTLETS; ++i) outlets_[i].id = i;
        }

        while (running_) {
            double temp  = readCpuTemp();
            int    mem   = readMemUsedPct();
            auto   tstate = thermal_.update(temp);

            cpu_temp_c_   = temp;
            mem_used_pct_ = mem;
            thermal_state_= tstate;

            // Update outlet telemetry
            double t = std::chrono::duration<double>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            {
                std::lock_guard<std::mutex> lk(mu_);
                for (auto& o : outlets_) {
                    // Sinusoidal load profile (0.5–3.5A) + noise
                    double base_a = 2.0 + 1.5 * std::sin(t * 0.1 + o.id * 0.8);
                    o.voltage_v = 120.0 + v_noise(rng);
                    o.current_a = std::max(0.0, base_a + i_noise(rng));
                    // CRITICAL: shed two highest-load outlets
                    if (tstate == ThermalState::CRITICAL) o.active = (o.id < 6);
                    else o.active = true;
                }
            }

            // Build JSON payload including per-outlet data
            std::string state_str = thermalStateName(tstate);
            std::string outlets_json = "[";
            {
                std::lock_guard<std::mutex> lk(mu_);
                for (int i = 0; i < PDU_OUTLETS; ++i) {
                    auto& o = outlets_[i];
                    if (i) outlets_json += ",";
                    outlets_json +=
                        "{\"id\":" + std::to_string(o.id) +
                        ",\"voltage_v\":" + std::to_string(o.voltage_v) +
                        ",\"current_a\":" + std::to_string(o.current_a) +
                        ",\"watts\":" + std::to_string(o.watts()) +
                        ",\"active\":" + (o.active ? "true" : "false") + "}";
                }
            }
            outlets_json += "]";
            std::string payload =
                "{\"cpu_temp_c\":" + std::to_string(temp) +
                ",\"mem_used_pct\":" + std::to_string(mem) +
                ",\"thermal_state\":\"" + state_str + "\"" +
                ",\"outlets\":" + outlets_json + "}";

            emit(EventType::TELEMETRY, payload);

            if (tstate == ThermalState::SHUTDOWN)
                emit(EventType::ERROR, "{\"error\":\"thermal_shutdown\"}");

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    // Read CPU temperature from Linux thermal sysfs (m°C → °C)
    static double readCpuTemp() {
#ifdef __linux__
        std::ifstream f("/sys/class/thermal/thermal_zone0/temp");
        int millideg = 0;
        if (f >> millideg) return millideg / 1000.0;
#endif
        // Synthetic: slow ramp 30→65°C over time (CI / macOS)
        static double t = 30.0, dir = 0.05;
        t += dir;
        if (t > 65.0 || t < 30.0) dir = -dir;
        return t;
    }

    // Read memory usage percent from /proc/meminfo
    static int readMemUsedPct() {
#ifdef __linux__
        std::ifstream f("/proc/meminfo");
        std::string line;
        long total = 0, avail = 0;
        while (std::getline(f, line)) {
            if (line.substr(0, 9)  == "MemTotal:") total = std::stol(line.substr(9));
            if (line.substr(0, 13) == "MemAvailable:") avail = std::stol(line.substr(13));
        }
        if (total > 0) return static_cast<int>(100 * (total - avail) / total);
#endif
        static int m = 40;
        m = (m + 1) % 80 + 20;
        return m;
    }

    mutable std::mutex mu_;
    std::vector<Outlet> outlets_;
    std::atomic<double>       cpu_temp_c_{30.0};
    std::atomic<int>          mem_used_pct_{40};
    std::atomic<ThermalState> thermal_state_{ThermalState::NORMAL};
    ThermalFsm                thermal_;
    std::atomic<bool>         running_{false};
    std::thread               thread_;
};
