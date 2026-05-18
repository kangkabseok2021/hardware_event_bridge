cmake_minimum_required(VERSION 3.21)
project(hardware_event_bridge CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Core library shared between main binary and tests
add_library(bridge_core STATIC
  src/DeviceManager.cpp
  src/SqliteLogger.cpp
  src/RestApi.cpp
)
target_include_directories(bridge_core PUBLIC src third_party)
target_link_libraries(bridge_core PUBLIC Threads::Threads SQLite::SQLite3)
target_compile_options(bridge_core PRIVATE -Wall -Wextra)

# Main daemon
add_executable(hardware_event_bridge src/main.cpp)
target_link_libraries(hardware_event_bridge PRIVATE bridge_core)

# Tests
enable_testing()
add_subdirectory(tests)
