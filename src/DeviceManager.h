#pragma once
#include "DeviceState.h"
#include "Event.h"
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Thread-safe device registry.
// Reads are lock-free via shared_mutex (multiple concurrent readers).
// Writes acquire an exclusive lock only during state update.
class DeviceManager {
public:
    void registerDevice(const DeviceId& id, const std::string& type);

    // Called by the Observer pipeline on every Event.
    void onEvent(const Event& event);

    // O(1) lookup — shared lock allows concurrent REST API reads.
    std::optional<DeviceState> getDevice(const DeviceId& id) const;
    std::vector<DeviceState>   getAllDevices() const;

    size_t deviceCount() const;

private:
    mutable std::shared_mutex                       mu_;
    std::unordered_map<DeviceId, DeviceState>       devices_;

    long long nowMs() const;
};
