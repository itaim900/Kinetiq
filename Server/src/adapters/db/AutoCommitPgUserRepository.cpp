#include "adapters/db/AutoCommitPgUserRepository.h"

#include "adapters/db/PgUserRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

kinetiq::domain::User AutoCommitPgUserRepository::save(const kinetiq::domain::User& user) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgUserRepository repo(tx);
    kinetiq::domain::User result = repo.save(user);
    tx.commit();
    return result;
}

std::optional<kinetiq::domain::User> AutoCommitPgUserRepository::findByEmail(
    const std::string& email) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgUserRepository repo(tx);
    auto result = repo.findByEmail(email);
    tx.commit();
    return result;
}

std::optional<kinetiq::domain::User> AutoCommitPgUserRepository::findById(long long id) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgUserRepository repo(tx);
    auto result = repo.findById(id);
    tx.commit();
    return result;
}

void AutoCommitPgUserRepository::update(const kinetiq::domain::User& user) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgUserRepository repo(tx);
    repo.update(user);
    tx.commit();
}

}  // namespace kinetiq::db
