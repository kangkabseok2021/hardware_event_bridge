cmake_minimum_required(VERSION 3.21)
project(pkv_claims_engine CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)

include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# Domain logic — pure C++, no external deps
add_library(pkv_domain STATIC
  src/ClaimValidator.cpp
  src/ReimbursementCalculator.cpp
)
target_include_directories(pkv_domain PUBLIC include)
target_compile_options(pkv_domain PRIVATE -Wall -Wextra)

# Data layer — libpqxx
add_library(pkv_data STATIC
  src/PolicyRepository.cpp
  src/ClaimRepository.cpp
)
target_include_directories(pkv_data PUBLIC include src)
target_link_libraries(pkv_data PUBLIC pkv_domain PkgConfig::PQXX Threads::Threads)
target_compile_options(pkv_data PRIVATE -Wall -Wextra)

# REST server — cpp-httplib + nlohmann/json (single-header, from ../third_party)
add_library(pkv_server STATIC src/ClaimsServer.cpp)
target_include_directories(pkv_server PUBLIC include src
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party)
target_link_libraries(pkv_server PUBLIC pkv_domain pkv_data)
target_compile_options(pkv_server PRIVATE -Wall -Wextra)

# Main executable
add_executable(pkv_claims_engine src/main.cpp)
target_link_libraries(pkv_claims_engine PRIVATE pkv_server)

# Tests (21 GoogleTests: 8 validator + 7 calculator + 6 DB integration)
enable_testing()
add_executable(test_pkv
  tests/test_validator.cpp
  tests/test_calculator.cpp
  tests/test_db_integration.cpp
)
target_include_directories(test_pkv PRIVATE include src)
target_link_libraries(test_pkv PRIVATE pkv_domain pkv_data GTest::gtest_main)
target_compile_options(test_pkv PRIVATE -Wall -Wextra)
gtest_discover_tests(test_pkv)
