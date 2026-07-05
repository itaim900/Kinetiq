#include "support/TestDb.h"

#include "adapters/db/Migrator.h"
#include "config/Env.h"

#include <exception>
#include <iostream>

#ifndef FITAPP_MIGRATIONS_DIR
#error "FITAPP_MIGRATIONS_DIR must be defined by the build (path to Server/migrations)"
#endif

namespace fitapp::testing {

namespace {

std::string connStringForDb(const std::string& dbname) {
    return "host=" + fitapp::envOr("DB_HOST", "127.0.0.1") +
           " port=" + fitapp::envOr("DB_PORT", "5432") +
           " user=" + fitapp::envOr("DB_USER", "fitapp") +
           " password=" + fitapp::envOr("DB_PASSWORD", "fitapp") +
           " dbname=" + dbname;
}

std::string testDbName() { return fitapp::envOr("TEST_DB_NAME", "fitapp_test"); }

}  // namespace

std::string testConnString() { return connStringForDb(testDbName()); }
std::string maintenanceConnString() { return connStringForDb("postgres"); }

bool ensureTestDatabaseReady() {
    const std::string dbName = testDbName();

    // Create the test database if needed (CREATE DATABASE can't run inside a
    // transaction, so use an autocommit nontransaction).
    try {
        pqxx::connection admin(maintenanceConnString());
        pqxx::nontransaction ntx(admin);
        const auto existing =
            ntx.exec("SELECT 1 FROM pg_database WHERE datname = " + ntx.quote(dbName));
        if (existing.empty()) {
            ntx.exec("CREATE DATABASE " + admin.quote_name(dbName));
        }
    } catch (const std::exception& e) {
        std::cerr << "[testdb] Postgres unreachable; integration tests will be skipped: "
                  << e.what() << std::endl;
        return false;
    }

    // Apply migrations to the test DB (idempotent).
    try {
        pqxx::connection conn(testConnString());
        fitapp::db::Migrator migrator(conn, FITAPP_MIGRATIONS_DIR);
        migrator.run();
    } catch (const std::exception& e) {
        std::cerr << "[testdb] migrating test DB failed: " << e.what() << std::endl;
        return false;
    }

    return true;
}

}  // namespace fitapp::testing

std::unique_ptr<pqxx::connection> DbTest::conn_;
bool DbTest::available_ = false;

void DbTest::SetUpTestSuite() {
    available_ = fitapp::testing::ensureTestDatabaseReady();
    if (available_) {
        conn_ = std::make_unique<pqxx::connection>(fitapp::testing::testConnString());
    }
}

void DbTest::TearDownTestSuite() { conn_.reset(); }

void DbTest::SetUp() {
    if (available_) {
        tx_ = std::make_unique<pqxx::work>(*conn_);
    }
}

void DbTest::TearDown() {
    if (tx_) {
        tx_->abort();  // roll back everything the test did
        tx_.reset();
    }
}
