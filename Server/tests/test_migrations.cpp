// Integration tests that exercise the real schema through the migration runner
// and the DbTest harness. These prove: (1) migrations create the core tables,
// (2) the per-test transaction rollback keeps tests isolated.
//
// They run against a live Postgres (the dev container is enough). When no DB is
// reachable they skip, so `ctest` still passes on a machine without Postgres.

#include "support/TestDb.h"

#include <gtest/gtest.h>

#include <string>

// The migrator should have created the core v1 tables.
TEST_F(DbTest, CoreTablesExist) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    for (const std::string table : {"users", "exercises", "workouts",
                                    "workout_exercises", "generation_requests",
                                    "schema_migrations"}) {
        const auto r = tx().exec("SELECT to_regclass('public." + table + "')");
        EXPECT_FALSE(r[0][0].is_null()) << "missing table: " << table;
    }
}

// Both baseline migrations should be recorded as applied.
TEST_F(DbTest, MigrationsRecorded) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    const auto r = tx().exec(
        "SELECT count(*) FROM schema_migrations "
        "WHERE version IN ('0001_init', '0002_core_schema')");
    EXPECT_EQ(r[0][0].as<int>(), 2);
}

// A row inserted in one test must not leak into the next (rollback isolation).
// Two tests insert the SAME email; if either leaked, the UNIQUE constraint would
// make the second insert fail.
TEST_F(DbTest, InsertIsRolledBack_A) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    tx().exec("INSERT INTO users (email, password_hash) VALUES ('iso@test.local', 'x')");
    const auto r = tx().exec("SELECT count(*) FROM users WHERE email = 'iso@test.local'");
    EXPECT_EQ(r[0][0].as<int>(), 1);
}

TEST_F(DbTest, InsertIsRolledBack_B) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    // Would throw a unique-violation if test A's insert had been committed.
    tx().exec("INSERT INTO users (email, password_hash) VALUES ('iso@test.local', 'x')");
    const auto r = tx().exec("SELECT count(*) FROM users WHERE email = 'iso@test.local'");
    EXPECT_EQ(r[0][0].as<int>(), 1);
}

// CHECK constraints from the schema are enforced (e.g. invalid experience_level).
TEST_F(DbTest, RejectsInvalidCheckConstraint) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    EXPECT_THROW(
        tx().exec("INSERT INTO users (email, password_hash, experience_level) "
                  "VALUES ('bad@test.local', 'x', 'wizard')"),
        std::exception);
}
