#pragma once
#include "DeviceManager.h"
#include "SqliteLogger.h"
#include <memory>
#include <string>
#include <thread>

// Thin REST API wrapper over httplib.
// Runs in a background thread; reads device state via DeviceManager
// (shared_mutex — lock-free concurrent reads).
class RestApi {
public:
    RestApi(DeviceManager& mgr, SqliteLogger& logger, int port = 8080);
    ~RestApi();

    void start();
    void stop();

private:
    DeviceManager& mgr_;
    SqliteLogger&  logger_;
    int            port_;

    struct Server;             // pimpl — keeps httplib.h out of this header
    std::unique_ptr<Server> svr_;
    std::thread              thread_;
};
