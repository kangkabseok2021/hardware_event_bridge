#include "DeviceManager.h"
#include <chrono>
#include <mutex>
#include <optional>
#include <shared_mutex>

long long DeviceManager::nowMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void DeviceManager::registerDevice(const DeviceId& id, const std::string& type) {
    std::unique_lock lk(mu_);
    DeviceState s;
    s.device_id   = id;
    s.device_type = type;
    s.status      = DeviceStatus::IDLE;
    devices_[id]  = std::move(s);
}

void DeviceManager::onEvent(const Event& event) {
    std::unique_lock lk(mu_);
    auto it = devices_.find(event.device_id);
    if (it == devices_.end()) return;

    auto& state = it->second;
    state.last_payload = event.payload;
    state.last_seen_ms = nowMs();

    switch (event.type) {
        case EventType::STATE_CHANGE:
            state.status = DeviceStatus::CONNECTED; break;
        case EventType::ERROR:
            state.status = DeviceStatus::ERROR; break;
        case EventType::TELEMETRY:
        case EventType::HEARTBEAT:
            if (state.status == DeviceStatus::IDLE)
                state.status = DeviceStatus::CONNECTED;
            break;
    }
}

std::optional<DeviceState> DeviceManager::getDevice(const DeviceId& id) const {
    std::shared_lock lk(mu_);
    auto it = devices_.find(id);
    if (it == devices_.end()) return std::nullopt;
    return it->second;
}

std::vector<DeviceState> DeviceManager::getAllDevices() const {
    std::shared_lock lk(mu_);
    std::vector<DeviceState> out;
    out.reserve(devices_.size());
    for (auto& [id, s] : devices_) out.push_back(s);
    return out;
}

size_t DeviceManager::deviceCount() const {
    std::shared_lock lk(mu_);
    return devices_.size();
}
