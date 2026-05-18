#include "SqliteLogger.h"
#include <chrono>
#include <sqlite3.h>
#include <stdexcept>

using namespace std::chrono_literals;

SqliteLogger::SqliteLogger(const std::string& db_path) : db_path_(db_path) {
    if (sqlite3_open(db_path_.c_str(), reinterpret_cast<sqlite3**>(&db_)) != SQLITE_OK)
        throw std::runtime_error("Cannot open SQLite DB: " + db_path_);
    initSchema();
    running_ = true;
    writer_ = std::thread([this]{ writerLoop(); });
}

SqliteLogger::~SqliteLogger() {
    running_ = false;
    if (writer_.joinable()) writer_.join();
    if (db_) sqlite3_close(reinterpret_cast<sqlite3*>(db_));
}

void SqliteLogger::initSchema() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS events (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id   TEXT    NOT NULL,
            event_type  TEXT    NOT NULL,
            payload     TEXT,
            ts_ms       INTEGER NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_device_ts
            ON events (device_id, ts_ms DESC);
    )";
    char* err = nullptr;
    sqlite3_exec(reinterpret_cast<sqlite3*>(db_), sql, nullptr, nullptr, &err);
    if (err) { sqlite3_free(err); }
}

void SqliteLogger::logEvent(const Event& event) {
    std::lock_guard lk(queue_mu_);
    queue_.push(event);
}

void SqliteLogger::flush() {
    std::vector<Event> batch;
    {
        std::lock_guard lk(queue_mu_);
        while (!queue_.empty()) {
            batch.push_back(std::move(queue_.front()));
            queue_.pop();
        }
    }
    if (!batch.empty()) writeBatch(batch);
}

void SqliteLogger::writerLoop() {
    while (running_) {
        std::this_thread::sleep_for(100ms);
        flush();
    }
    flush(); // drain on exit
}

void SqliteLogger::writeBatch(std::vector<Event>& batch) {
    auto* db = reinterpret_cast<sqlite3*>(db_);
    sqlite3_exec(db, "BEGIN", nullptr, nullptr, nullptr);
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db,
        "INSERT INTO events (device_id, event_type, payload, ts_ms) VALUES (?,?,?,?)",
        -1, &stmt, nullptr);

    for (auto& e : batch) {
        long long ts = std::chrono::duration_cast<std::chrono::milliseconds>(
            e.timestamp.time_since_epoch()).count();
        sqlite3_bind_text(stmt, 1, e.device_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, eventTypeName(e.type).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, e.payload.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, ts);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
}

std::vector<std::tuple<std::string,std::string,std::string,std::string>>
SqliteLogger::recentEvents(int limit) const {
    std::vector<std::tuple<std::string,std::string,std::string,std::string>> result;
    auto* db = reinterpret_cast<sqlite3*>(db_);
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db,
        "SELECT device_id, event_type, payload, ts_ms FROM events "
        "ORDER BY ts_ms DESC LIMIT ?",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.emplace_back(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
            std::to_string(sqlite3_column_int64(stmt, 3))
        );
    }
    sqlite3_finalize(stmt);
    return result;
}
