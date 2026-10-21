cmake_minimum_required(VERSION 3.20)
project(kinetiq_backend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# How we obtain libpqxx:
#  * Local dev (MSYS2): use the prebuilt pacman package — fast (default, OFF).
#  * CI / Docker (Linux): build libpqxx from source, because Ubuntu's prebuilt
#    libpqxx-dev 7.8.1 has a header/library mismatch around std::source_location
#    that makes linking fail. Building from source keeps headers + lib consistent.
option(KINETIQ_FETCH_LIBPQXX "Build libpqxx from source instead of using a prebuilt package" OFF)

if(KINETIQ_FETCH_LIBPQXX)
    include(FetchContent)
    set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)  # don't build libpqxx's own test suite
    FetchContent_Declare(libpqxx
        GIT_REPOSITORY https://github.com/jtv/libpqxx.git
        GIT_TAG 7.9.2
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(libpqxx)
    add_library(kinetiq_pqxx INTERFACE)
    target_link_libraries(kinetiq_pqxx INTERFACE pqxx)
else()
    # Prebuilt libpqxx (+ libpq); headers are on the default include path.
    find_library(PQXX_LIB NAMES pqxx libpqxx REQUIRED)
    find_library(PQ_LIB   NAMES pq   libpq   REQUIRED)
    add_library(kinetiq_pqxx INTERFACE)
    target_link_libraries(kinetiq_pqxx INTERFACE ${PQXX_LIB} ${PQ_LIB})
endif()

# --- auth dependencies (Phase 2) ---
# argon2 (password hashing) + jwt-cpp (JWT). Both must build on MSYS2 AND Linux
# (CI/Docker). argon2 is a prebuilt system lib on both; jwt-cpp is header-only
# via FetchContent (ABI-safe), and needs OpenSSL's libcrypto for signing.
#
# Slice 0 wired these into the TEST target only, to verify the toolchain with
# a smoke test. Slice 1 (#17) promotes them onto kinetiq_server itself, now
# that Argon2PasswordHasher/JwtTokenService actually need them at runtime --
# so this FetchContent block must be available unconditionally, not just
# under KINETIQ_BUILD_TESTS.
include(FetchContent)
set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JWT_DISABLE_PICOJSON OFF CACHE BOOL "" FORCE)
FetchContent_Declare(jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG v0.7.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(jwt-cpp)

function(kinetiq_add_auth_deps target)
    find_package(OpenSSL REQUIRED)
    find_library(ARGON2_LIB NAMES argon2 libargon2 REQUIRED)
    find_path(ARGON2_INCLUDE_DIR argon2.h REQUIRED)
    target_include_directories(${target} PRIVATE ${ARGON2_INCLUDE_DIR})
    target_link_libraries(${target} PRIVATE ${ARGON2_LIB} OpenSSL::Crypto jwt-cpp::jwt-cpp)
endfunction()

# --- application server ---
add_executable(kinetiq_server
    src/main.cpp
    src/adapters/db/Migrator.cpp
    src/adapters/db/PgUserRepository.cpp
    src/adapters/db/AutoCommitPgUserRepository.cpp
    src/adapters/db/PgInjuryRepository.cpp
    src/adapters/db/AutoCommitPgInjuryRepository.cpp
    src/adapters/db/PgCatalogRepository.cpp
    src/adapters/db/AutoCommitPgCatalogRepository.cpp
    src/adapters/crypto/Argon2PasswordHasher.cpp
    src/adapters/crypto/JwtTokenService.cpp
    src/http/controllers/AuthController.cpp
    src/http/controllers/UserController.cpp
    src/http/controllers/InjuryController.cpp)
target_include_directories(kinetiq_server PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/third_party/crow)
target_link_libraries(kinetiq_server PRIVATE
    kinetiq_pqxx
    Threads::Threads)
kinetiq_add_auth_deps(kinetiq_server)

# Crow/asio needs the Win32 socket libraries on Windows only. On Linux/macOS
# the sockets API lives in libc, so these must not be linked there.
if(WIN32)
    target_link_libraries(kinetiq_server PRIVATE ws2_32 wsock32 mswsock)
endif()

# --- tests ---
# Skipped in the Docker image build (no GoogleTest needed there); ON for local
# dev and CI so `ctest` runs the suite.
option(KINETIQ_BUILD_TESTS "Build the unit test suite" ON)
if(KINETIQ_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)

    add_executable(kinetiq_tests
        tests/test_main.cpp
        tests/test_auth_deps.cpp
        tests/test_migrations.cpp
        tests/test_http_harness.cpp
        tests/test_password_policy.cpp
        tests/test_profile_policy.cpp
        tests/test_injury_policy.cpp
        tests/test_injury_repository.cpp
        tests/test_catalog_repository.cpp
        tests/test_injury_service.cpp
        tests/test_user_repository.cpp
        tests/test_pg_user_repository.cpp
        tests/test_password_hasher.cpp
        tests/test_token_service.cpp
        tests/test_authenticator.cpp
        tests/test_register_user.cpp
        tests/test_login_user.cpp
        tests/test_change_password.cpp
        tests/test_update_profile.cpp
        tests/test_auth_controller.cpp
        tests/test_user_controller.cpp
        tests/test_injury_controller.cpp
        tests/test_auto_commit_pg_user_repository.cpp
        tests/test_pg_injury_repository.cpp
        tests/test_auto_commit_pg_injury_repository.cpp
        tests/test_pg_catalog_repository.cpp
        tests/test_auto_commit_pg_catalog_repository.cpp
        tests/support/TestDb.cpp
        src/adapters/db/Migrator.cpp
        src/adapters/db/PgUserRepository.cpp
        src/adapters/db/AutoCommitPgUserRepository.cpp
        src/adapters/db/PgInjuryRepository.cpp
        src/adapters/db/AutoCommitPgInjuryRepository.cpp
        src/adapters/db/PgCatalogRepository.cpp
        src/adapters/db/AutoCommitPgCatalogRepository.cpp
        src/adapters/crypto/Argon2PasswordHasher.cpp
        src/adapters/crypto/JwtTokenService.cpp
        src/http/controllers/AuthController.cpp
        src/http/controllers/UserController.cpp
        src/http/controllers/InjuryController.cpp)
    target_include_directories(kinetiq_tests PRIVATE
        src tests
        ${CMAKE_SOURCE_DIR}/third_party/crow)  # Crow for the in-process HTTP harness
    target_link_libraries(kinetiq_tests PRIVATE
        GTest::gtest GTest::gtest_main
        kinetiq_pqxx
        Threads::Threads)
    # Crow/asio needs the Win32 socket libs on Windows (even when we never open a
    # socket — handle_full links them in). No-op on Linux.
    if(WIN32)
        target_link_libraries(kinetiq_tests PRIVATE ws2_32 wsock32 mswsock)
    endif()
    # Absolute path so integration tests find the SQL regardless of ctest's CWD.
    target_compile_definitions(kinetiq_tests PRIVATE
        KINETIQ_MIGRATIONS_DIR="${CMAKE_SOURCE_DIR}/migrations")
    kinetiq_add_auth_deps(kinetiq_tests)

    include(GoogleTest)
    gtest_discover_tests(kinetiq_tests)
endif()
