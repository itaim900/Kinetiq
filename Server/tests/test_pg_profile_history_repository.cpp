// Profile history: migration 0006 + PgProfileHistoryRepository (DbTest rollback).

#include "support/TestDb.h"

#include <gtest/gtest.h>

#include "adapters/db/PgProfileHistoryRepository.h"

#include <pqxx/pqxx>

using kinetiq::db::PgProfileHistoryRepository;

namespace {
long long seedUser(pqxx::work& tx, const std::string& email) {
    return tx.exec("INSERT INTO users (email, password_hash) VALUES ($1, 'x') RETURNING id",
                   pqxx::params{email})[0][0].as<long long>();
}
}  // namespace

TEST_F(DbTest, PgProfileHistory_RecordsRow) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    const long long userId = seedUser(tx(), "hist@example.com");
    PgProfileHistoryRepository history(tx());
    history.record(userId, "weight_kg", std::string("74"), std::nullopt);

    const auto rows = tx().exec(
        "SELECT field, old_value, new_value, changed_at FROM user_profile_history WHERE user_id = $1",
        pqxx::params{userId});
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0]["field"].as<std::string>(), "weight_kg");
    EXPECT_EQ(rows[0]["old_value"].as<std::string>(), "74");
    EXPECT_TRUE(rows[0]["new_value"].is_null());
    EXPECT_FALSE(rows[0]["changed_at"].is_null());
}

TEST_F(DbTest, PgProfileHistory_RejectsUntrackedField) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    const long long userId = seedUser(tx(), "hist-bad@example.com");
    PgProfileHistoryRepository history(tx());
    EXPECT_THROW(history.record(userId, "unit_preference", std::nullopt, std::string("metric")),
                 pqxx::sql_error);
}

TEST_F(DbTest, PgProfileHistory_DeletedWithUser) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    const long long userId = seedUser(tx(), "hist-cascade@example.com");
    PgProfileHistoryRepository(tx()).record(userId, "sex", std::nullopt, std::string("female"));
    tx().exec("DELETE FROM users WHERE id = $1", pqxx::params{userId});
    EXPECT_EQ(tx().exec("SELECT count(*) FROM user_profile_history WHERE user_id = $1",
                        pqxx::params{userId})[0][0].as<long long>(),
              0);
}
