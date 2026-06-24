cmake_minimum_required(VERSION 3.20)
project(fit_app_backend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# libpqxx (+ libpq) come prebuilt from MSYS2; headers are on the default
# compiler include path, so we only need to locate the import libraries.
find_library(PQXX_LIB NAMES pqxx libpqxx REQUIRED)
find_library(PQ_LIB   NAMES pq   libpq   REQUIRED)

# --- application server ---
add_executable(fitapp_server src/main.cpp)
target_include_directories(fitapp_server PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/third_party/crow)
target_link_libraries(fitapp_server PRIVATE
    ${PQXX_LIB} ${PQ_LIB}
    Threads::Threads
    ws2_32 wsock32 mswsock)  # Windows sockets for Crow/asio

# --- tests ---
enable_testing()
find_package(GTest REQUIRED)
add_executable(fitapp_tests tests/test_main.cpp)
target_include_directories(fitapp_tests PRIVATE src)
target_link_libraries(fitapp_tests PRIVATE GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(fitapp_tests)
