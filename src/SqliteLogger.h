#pragma once
#include "Event.h"
#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

// Async SQLite logger: hardware events are pushed to a lock-free queue by
// the Observer pipeline; a dedicated writer thread drains and batch-inserts
// them every 100ms. Errors are flushed immediately.
class SqliteLogger {
public:
    explicit SqliteLogger(const std::string& db_path);
    ~SqliteLogger();

    void logEvent(const Event& event);
    void flush();

    // Query helpers for REST API
    std::vector<std::tuple<std::string,std::string,std::string,std::string>>
        recentEvents(int limit = 20) const;

private:
    void writerLoop();
    void initSchema();
    void writeBatch(std::vector<Event>& batch);

    std::string       db_path_;
    void*             db_{nullptr};    // sqlite3* — opaque to avoid header dep

    mutable std::mutex     queue_mu_;
    std::queue<Event>      queue_;
    std::atomic<bool>      running_{false};
    std::thread            writer_;
};
