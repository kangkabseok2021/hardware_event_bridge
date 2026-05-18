#pragma once
#include "IDevice.h"
#include "SerialDevice.h"
#include "I2cDevice.h"
#include <memory>
#include <stdexcept>
#include <string>

// Factory pattern: creates the correct IDevice subclass from a type string.
// Adding a new hardware type requires only a new subclass + one case here —
// the EventEngine, DeviceManager, and RestApi are untouched.
class DeviceFactory {
public:
    static std::unique_ptr<IDevice> create(
        const std::string& type,
        const DeviceId&    id,
        EventEngine&       engine)
    {
        if (type == "serial") return std::make_unique<SerialDevice>(id, engine);
        if (type == "i2c")    return std::make_unique<I2cDevice>(id, engine);
        throw std::invalid_argument("Unknown device type: " + type);
    }
};
