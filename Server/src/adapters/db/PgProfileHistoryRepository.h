#pragma once

#include "ports/IProfileHistoryRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

// Postgres-backed history log, inside a transaction it is handed (DbTest
// rollback in tests; per-call transaction via the AutoCommit wrapper in prod).
class PgProfileHistoryRepository : public kinetiq::ports::IProfileHistoryRepository {
public:
    explicit PgProfileHistoryRepository(pqxx::work& tx) : tx_(tx) {}

    void record(long long userId, const std::string& field,
                const std::optional<std::string>& oldValue,
                const std::optional<std::string>& newValue) override;

private:
    pqxx::work& tx_;
};

}  // namespace kinetiq::db
