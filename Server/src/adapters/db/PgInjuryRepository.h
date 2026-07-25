#pragma once

#include "domain/Injury.h"
#include "ports/IInjuryRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

// Postgres-backed IInjuryRepository. Operates within a transaction it is handed
// (composes with DbTest's rollback in tests, a per-op transaction in prod via
// AutoCommitPgInjuryRepository).
class PgInjuryRepository : public kinetiq::ports::IInjuryRepository {
public:
    explicit PgInjuryRepository(pqxx::work& tx) : tx_(tx) {}

    kinetiq::domain::Injury create(const kinetiq::domain::Injury& injury) override;
    std::vector<kinetiq::domain::Injury> listByUser(long long userId) override;
    std::optional<kinetiq::domain::Injury> findById(long long id) override;
    void update(const kinetiq::domain::Injury& injury) override;
    void deleteById(long long id) override;

private:
    pqxx::work& tx_;
};

}  // namespace kinetiq::db
