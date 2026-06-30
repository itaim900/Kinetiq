cmake_minimum_required(VERSION 3.20)
project(fit_app_backend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# How we obtain libpqxx:
#  * Local dev (MSYS2): use the prebuilt pacman package — fast (default, OFF).
#  * CI / Docker (Linux): build libpqxx from source, because Ubuntu's prebuilt
#    libpqxx-dev 7.8.1 has a header/library mismatch around std::source_location
#    that makes linking fail. Building from source keeps headers + lib consistent.
option(FITAPP_FETCH_LIBPQXX "Build libpqxx from source instead of using a prebuilt package" OFF)

if(FITAPP_FETCH_LIBPQXX)
    include(FetchContent)
    set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)  # don't build libpqxx's own test suite
    FetchContent_Declare(libpqxx
        GIT_REPOSITORY https://github.com/jtv/libpqxx.git
        GIT_TAG 7.9.2
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(libpqxx)
    add_library(fitapp_pqxx INTERFACE)
    target_link_libraries(fitapp_pqxx INTERFACE pqxx)
else()
    # Prebuilt libpqxx (+ libpq); headers are on the default include path.
    find_library(PQXX_LIB NAMES pqxx libpqxx REQUIRED)
    find_library(PQ_LIB   NAMES pq   libpq   REQUIRED)
    add_library(fitapp_pqxx INTERFACE)
    target_link_libraries(fitapp_pqxx INTERFACE ${PQXX_LIB} ${PQ_LIB})
endif()

# --- application server ---
add_executable(fitapp_server src/main.cpp)
target_include_directories(fitapp_server PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/third_party/crow)
target_link_libraries(fitapp_server PRIVATE
    fitapp_pqxx
    Threads::Threads)

# Crow/asio needs the Win32 socket libraries on Windows only. On Linux/macOS
# the sockets API lives in libc, so these must not be linked there.
if(WIN32)
    target_link_libraries(fitapp_server PRIVATE ws2_32 wsock32 mswsock)
endif()

# --- tests ---
# Skipped in the Docker image build (no GoogleTest needed there); ON for local
# dev and CI so `ctest` runs the suite.
option(FITAPP_BUILD_TESTS "Build the unit test suite" ON)
if(FITAPP_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_executable(fitapp_tests tests/test_main.cpp)
    target_include_directories(fitapp_tests PRIVATE src)
    target_link_libraries(fitapp_tests PRIVATE GTest::gtest GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(fitapp_tests)
endif()
