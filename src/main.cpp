#include "EventEngine.h"
#include "DeviceManager.h"
#include "SqliteLogger.h"
#include "RestApi.h"
#include "devices/DeviceFactory.h"
#include <csignal>
#include <iostream>
#include <thread>
#include <vector>

static std::atomic<bool> g_running{true};
void sigHandler(int) { g_running = false; }

int main() {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    // Dependency injection — only main.cpp knows all subsystems
    EventEngine   engine;
    DeviceManager manager;
    SqliteLogger  logger("events.db");
    RestApi       api(manager, logger, 8080);

    // Wire Observer: every event updates DeviceManager and logs to SQLite
    engine.subscribe([&](const Event& e){ manager.onEvent(e); });
    engine.subscribe([&](const Event& e){ logger.logEvent(e); });

    // Factory creates devices from config (in production: read from YAML/env)
    struct DeviceConfig { std::string id, type; };
    std::vector<DeviceConfig> cfg = {
        {"serial-0", "serial"},
        {"i2c-0",    "i2c"},
        {"i2c-1",    "i2c"},
    };

    std::vector<std::unique_ptr<IDevice>> devices;
    for (auto& c : cfg) {
        manager.registerDevice(c.id, c.type);
        auto dev = DeviceFactory::create(c.type, c.id, engine);
        dev->start();
        devices.push_back(std::move(dev));
    }

    api.start();
    std::cout << "[bridge] Running — REST API on :8080\n"
              << "[bridge] Endpoints: GET /devices  /devices/{id}  /events  /health\n";

    while (g_running) std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "[bridge] Shutting down...\n";
    for (auto& d : devices) d->stop();
    api.stop();
    logger.flush();
    return 0;
}
