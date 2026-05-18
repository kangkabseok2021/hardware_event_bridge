add_executable(test_bridge
  test_event_engine.cpp
  test_device_manager.cpp
  test_sqlite_logger.cpp
  test_device_factory.cpp
)
target_link_libraries(test_bridge PRIVATE bridge_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(test_bridge)
