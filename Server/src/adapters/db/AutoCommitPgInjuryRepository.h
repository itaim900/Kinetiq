#pragma once

#include "ports/IInjuryRepository.h"

#include <string>

namespace kinetiq::db {

// Production IInjuryRepository: opens a fresh connection + transaction per call,
// delegates to PgInjuryRepository, commits. Same rationale as
// AutoCommitPgUserRepository (a Crow route captures its deps for the server's
// lifetime; a pqxx transaction is per-operation and unsafe to share across
// concurrent requests).
class AutoCommitPgInjuryRepository : public kinetiq::ports::IInjuryRepository {
public:
    explicit AutoCommitPgInjuryRepository(std::string connString)
        : connString_(std::move(connString)) {}

    kinetiq::domain::Injury create(const kinetiq::domain::Injury& injury) override;
    std::vector<kinetiq::domain::Injury> listByUser(long long userId) override;
    std::optional<kinetiq::domain::Injury> findById(long long id) override;
    void update(const kinetiq::domain::Injury& injury) override;
    void deleteById(long long id) override;

private:
    std::string connString_;
};

}  // namespace kinetiq::db
