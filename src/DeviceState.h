#pragma once
#include <string>

enum class DeviceStatus { CONNECTED, DISCONNECTED, ERROR, IDLE };

struct DeviceState {
    std::string  device_id;
    std::string  device_type;   // "serial" | "usb" | "i2c"
    DeviceStatus status{DeviceStatus::IDLE};
    std::string  last_payload;
    long long    last_seen_ms{0};
};

inline std::string statusName(DeviceStatus s) {
    switch (s) {
        case DeviceStatus::CONNECTED:    return "connected";
        case DeviceStatus::DISCONNECTED: return "disconnected";
        case DeviceStatus::ERROR:        return "error";
        case DeviceStatus::IDLE:         return "idle";
    }
    return "unknown";
}
