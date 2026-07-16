#pragma once

#include "domain/User.h"
#include "ports/IUserRepository.h"

#include <pqxx/pqxx>

namespace fitapp::db {

// Postgres-backed IUserRepository. Operates within a transaction it is
// handed rather than one it owns, so it composes with whatever transaction
// scope the caller is using: DbTest's per-test rollback transaction in
// tests, a per-request transaction in production. A pqxx::connection can
// only have one transaction open at a time, so this class must not try to
// start its own.
class PgUserRepository : public fitapp::ports::IUserRepository {
public:
    explicit PgUserRepository(pqxx::work& tx) : tx_(tx) {}

    fitapp::domain::User save(const fitapp::domain::User& user) override;
    std::optional<fitapp::domain::User> findByEmail(const std::string& email) override;
    std::optional<fitapp::domain::User> findById(long long id) override;
    void update(const fitapp::domain::User& user) override;

private:
    pqxx::work& tx_;
};

}  // namespace fitapp::db
