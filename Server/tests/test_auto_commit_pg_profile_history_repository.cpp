// AutoCommitPgProfileHistoryRepository -- the one wired into main.cpp. Cleans
// up by deleting its user (history cascades).

#include "support/TestDb.h"

#include <gtest/gtest.h>

#include "adapters/db/AutoCommitPgProfileHistoryRepository.h"

#include <pqxx/pqxx>

using kinetiq::db::AutoCommitPgProfileHistoryRepository;

TEST(AutoCommitPgProfileHistoryRepositoryTest, RecordsOverOwnConnection) {
    if (!kinetiq::testing::ensureTestDatabaseReady()) GTEST_SKIP() << "no test database reachable";
    long long userId = 0;
    {
        pqxx::connection conn(kinetiq::testing::testConnString());
        pqxx::work tx(conn);
        userId = tx.exec("INSERT INTO users (email, password_hash) "
                         "VALUES ('hist-auto@example.com', 'x') RETURNING id")[0][0]
                     .as<long long>();
        tx.commit();
    }
    AutoCommitPgProfileHistoryRepository history(kinetiq::testing::testConnString());
    history.record(userId, "height_cm", std::nullopt, std::string("178"));

    pqxx::connection conn(kinetiq::testing::testConnString());
    pqxx::work tx(conn);
    EXPECT_EQ(tx.exec("SELECT count(*) FROM user_profile_history WHERE user_id = $1",
                      pqxx::params{userId})[0][0].as<long long>(),
              1);
    tx.exec("DELETE FROM users WHERE id = $1", pqxx::params{userId});
    tx.commit();
}
