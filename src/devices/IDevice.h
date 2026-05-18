#pragma once
#include "../Event.h"
#include "../EventEngine.h"
#include <string>

// Abstract device interface — Factory instantiates the correct subclass.
// Concrete drivers override start() / stop() only; emit() is inherited.
class IDevice {
public:
    explicit IDevice(DeviceId id, EventEngine& engine)
        : id_(std::move(id)), engine_(engine) {}
    virtual ~IDevice() = default;

    virtual void start() = 0;
    virtual void stop()  = 0;

    const DeviceId& id() const { return id_; }

protected:
    void emit(EventType type, std::string payload) {
        engine_.emit({ id_, type, std::move(payload) });
    }

    DeviceId     id_;
    EventEngine& engine_;
};
