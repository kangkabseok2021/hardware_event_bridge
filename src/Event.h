#pragma once
#include <chrono>
#include <string>

using DeviceId = std::string;

enum class EventType { TELEMETRY, STATE_CHANGE, ERROR, HEARTBEAT };

struct Event {
    DeviceId    device_id;
    EventType   type;
    std::string payload;     // JSON string
    std::chrono::system_clock::time_point timestamp{
        std::chrono::system_clock::now()
    };
};

inline std::string eventTypeName(EventType t) {
    switch (t) {
        case EventType::TELEMETRY:    return "TELEMETRY";
        case EventType::STATE_CHANGE: return "STATE_CHANGE";
        case EventType::ERROR:        return "ERROR";
        case EventType::HEARTBEAT:    return "HEARTBEAT";
    }
    return "UNKNOWN";
}
