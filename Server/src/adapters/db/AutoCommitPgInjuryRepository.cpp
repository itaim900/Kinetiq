#include "adapters/db/AutoCommitPgInjuryRepository.h"

#include "adapters/db/PgInjuryRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

kinetiq::domain::Injury AutoCommitPgInjuryRepository::create(
    const kinetiq::domain::Injury& injury) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgInjuryRepository repo(tx);
    auto result = repo.create(injury);
    tx.commit();
    return result;
}

std::vector<kinetiq::domain::Injury> AutoCommitPgInjuryRepository::listByUser(long long userId) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgInjuryRepository repo(tx);
    auto result = repo.listByUser(userId);
    tx.commit();
    return result;
}

std::optional<kinetiq::domain::Injury> AutoCommitPgInjuryRepository::findById(long long id) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgInjuryRepository repo(tx);
    auto result = repo.findById(id);
    tx.commit();
    return result;
}

void AutoCommitPgInjuryRepository::update(const kinetiq::domain::Injury& injury) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgInjuryRepository repo(tx);
    repo.update(injury);
    tx.commit();
}

void AutoCommitPgInjuryRepository::deleteById(long long id) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgInjuryRepository repo(tx);
    repo.deleteById(id);
    tx.commit();
}

}  // namespace kinetiq::db
