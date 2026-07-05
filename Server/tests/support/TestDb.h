#pragma once

#include <gtest/gtest.h>
#include <pqxx/pqxx>

#include <memory>
#include <string>

namespace fitapp::testing {

// Connection string for the integration-test database (a separate DB from the
// dev one so tests never touch real data). Built from DB_* env vars, with the
// database name from TEST_DB_NAME (default "fitapp_test").
std::string testConnString();

// Connection string to the "postgres" maintenance DB, used only to CREATE the
// test database if it doesn't exist yet.
std::string maintenanceConnString();

// Ensures the test database exists and all migrations are applied. Returns false
// if no Postgres server is reachable — callers should then GTEST_SKIP().
bool ensureTestDatabaseReady();

}  // namespace fitapp::testing

// Base fixture for integration tests that need a real database.
//
// Schema is created once for the whole suite (SetUpTestSuite). Every individual
// test runs inside a transaction that is rolled back in TearDown, so tests are
// isolated and leave the database exactly as they found it — fast, no cleanup.
//
// Usage:
//   TEST_F(DbTest, SomethingWithTheDb) {
//       if (!available()) GTEST_SKIP() << "no test database";
//       tx().exec("INSERT ...");
//       ...
//   }
class DbTest : public ::testing::Test {
protected:
    static void SetUpTestSuite();
    static void TearDownTestSuite();
    void SetUp() override;
    void TearDown() override;

    // True if a Postgres server was reachable during suite setup.
    static bool available() { return available_; }

    // The per-test transaction (rolled back automatically after the test).
    pqxx::work& tx() { return *tx_; }

private:
    static std::unique_ptr<pqxx::connection> conn_;
    static bool available_;
    std::unique_ptr<pqxx::work> tx_;
};
