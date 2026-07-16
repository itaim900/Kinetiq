#pragma once

#include "domain/User.h"
#include "ports/IUserRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

// Postgres-backed IUserRepository. Operates within a transaction it is
// handed rather than one it owns, so it composes with whatever transaction
// scope the caller is using: DbTest's per-test rollback transaction in
// tests, a per-request transaction in production. A pqxx::connection can
// only have one transaction open at a time, so this class must not try to
// start its own.
class PgUserRepository : public kinetiq::ports::IUserRepository {
public:
    explicit PgUserRepository(pqxx::work& tx) : tx_(tx) {}

    kinetiq::domain::User save(const kinetiq::domain::User& user) override;
    std::optional<kinetiq::domain::User> findByEmail(const std::string& email) override;
    std::optional<kinetiq::domain::User> findById(long long id) override;
    void update(const kinetiq::domain::User& user) override;

private:
    pqxx::work& tx_;
};

}  // namespace kinetiq::db
