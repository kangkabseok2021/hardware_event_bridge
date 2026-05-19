#include "RestApi.h"
#include "../third_party/httplib.h"
#include "../third_party/json.hpp"

struct RestApi::Server {
    httplib::Server http;
};

RestApi::RestApi(DeviceManager& mgr, SqliteLogger& logger, int port)
    : mgr_(mgr), logger_(logger), port_(port),
      svr_(std::make_unique<Server>()) {}

RestApi::~RestApi() { stop(); }

void RestApi::start() {
    // GET /devices — all device states
    svr_->http.Get("/devices", [this](const httplib::Request&, httplib::Response& res) {
        auto devices = mgr_.getAllDevices();
        nlohmann::json arr = nlohmann::json::array();
        for (auto& d : devices) {
            arr.push_back({
                {"id",          d.device_id},
                {"type",        d.device_type},
                {"status",      statusName(d.status)},
                {"last_payload", d.last_payload},
                {"last_seen_ms", d.last_seen_ms},
            });
        }
        res.set_content(arr.dump(), "application/json");
    });

    // GET /devices/:id — single device
    svr_->http.Get(R"(/devices/([^/]+))",
        [this](const httplib::Request& req, httplib::Response& res) {
            auto id = req.matches[1].str();
            auto dev = mgr_.getDevice(id);
            if (!dev) { res.status = 404; res.set_content("{}", "application/json"); return; }
            nlohmann::json j = {
                {"id",           dev->device_id},
                {"type",         dev->device_type},
                {"status",       statusName(dev->status)},
                {"last_payload", dev->last_payload},
                {"last_seen_ms", dev->last_seen_ms},
            };
            res.set_content(j.dump(), "application/json");
        });

    // GET /events?limit=N — recent event log
    svr_->http.Get("/events", [this](const httplib::Request& req, httplib::Response& res) {
        int limit = 20;
        if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
        auto rows = logger_.recentEvents(limit);
        nlohmann::json arr = nlohmann::json::array();
        for (auto& [dev, type, payload, ts] : rows)
            arr.push_back({{"device_id",dev},{"type",type},{"payload",payload},{"ts_ms",ts}});
        res.set_content(arr.dump(), "application/json");
    });

    // GET /api/power — PDU telemetry (outlet states, thermal FSM, CPU + memory)
    // Reads the last payload emitted by the power_monitor device.
    svr_->http.Get("/api/power", [this](const httplib::Request&, httplib::Response& res) {
        auto dev = mgr_.getDevice("power-monitor-0");
        if (!dev || dev->last_payload.empty()) {
            res.status = 503;
            res.set_content(R"({"error":"power monitor not ready"})", "application/json");
            return;
        }
        res.set_content(dev->last_payload, "application/json");
    });

    // GET /health
    svr_->http.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    thread_ = std::thread([this] {
        svr_->http.listen("0.0.0.0", port_);
    });
}

void RestApi::stop() {
    svr_->http.stop();
    if (thread_.joinable()) thread_.join();
}
