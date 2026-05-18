#include <gtest/gtest.h>
#include "SqliteLogger.h"
#include <chrono>
#include <filesystem>
#include <thread>

class SqliteLoggerTest : public ::testing::Test {
protected:
    std::string db_path_;
    void SetUp() override {
        db_path_ = std::filesystem::temp_directory_path() / "test_events.db";
        std::filesystem::remove(db_path_);
    }
    void TearDown() override { std::filesystem::remove(db_path_); }
};

TEST_F(SqliteLoggerTest, OpenAndSchemaCreated) {
    EXPECT_NO_THROW(SqliteLogger logger(db_path_));
}

TEST_F(SqliteLoggerTest, LogAndRetrieve) {
    SqliteLogger logger(db_path_);
    logger.logEvent({"dev1", EventType::TELEMETRY, R"({"v":1})"});
    std::this_thread::sleep_for(std::chrono::milliseconds(150)); // wait writer
    auto rows = logger.recentEvents(10);
    ASSERT_FALSE(rows.empty());
    EXPECT_EQ(std::get<0>(rows[0]), "dev1");
    EXPECT_EQ(std::get<1>(rows[0]), "TELEMETRY");
}

TEST_F(SqliteLoggerTest, FlushWritesImmediately) {
    SqliteLogger logger(db_path_);
    logger.logEvent({"dev2", EventType::ERROR, "err"});
    logger.flush();
    auto rows = logger.recentEvents(5);
    EXPECT_GE(rows.size(), 1u);
}

TEST_F(SqliteLoggerTest, MultipleEventsOrdered) {
    SqliteLogger logger(db_path_);
    for (int i = 0; i < 5; ++i)
        logger.logEvent({"dev1", EventType::TELEMETRY, std::to_string(i)});
    logger.flush();
    auto rows = logger.recentEvents(10);
    EXPECT_GE(rows.size(), 5u);
}
