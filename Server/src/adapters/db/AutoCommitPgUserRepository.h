#pragma once

#include "ports/IUserRepository.h"

#include <string>

namespace kinetiq::db {

// Production IUserRepository: opens a fresh Postgres connection + transaction
// for EACH call, delegates to PgUserRepository, commits, and closes --
// "auto-commit" in the classic DB sense (every operation is its own implicit
// transaction). Needed because a Crow route is registered once at server
// startup and captures its dependencies by reference for the server's whole
// lifetime (see http/controllers/AuthController.h) -- but a pqxx transaction
// is inherently per-operation and unsafe to share across the concurrent
// requests a multithreaded server handles. PgUserRepository itself keeps the
// "hand me an existing transaction" shape (used directly in tests, where a
// single DbTest-owned transaction must span the whole test for rollback
// isolation) -- this class wraps it rather than changing it.
//
// Known limitation, accepted for v1: multi-step use cases (e.g.
// ChangePassword's findById() then update()) do NOT get one atomic
// transaction across both calls -- each commits independently. The only way
// to observe this is a narrow, low-severity race (the same user sending two
// concurrent requests); closing it properly would mean threading a
// transaction through Crow's request lifecycle, which nothing in this
// codebase does yet.
class AutoCommitPgUserRepository : public kinetiq::ports::IUserRepository {
public:
    explicit AutoCommitPgUserRepository(std::string connString)
        : connString_(std::move(connString)) {}

    kinetiq::domain::User save(const kinetiq::domain::User& user) override;
    std::optional<kinetiq::domain::User> findByEmail(const std::string& email) override;
    std::optional<kinetiq::domain::User> findById(long long id) override;
    void update(const kinetiq::domain::User& user) override;

private:
    std::string connString_;
};

}  // namespace kinetiq::db
