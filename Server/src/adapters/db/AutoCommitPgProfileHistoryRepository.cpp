#include "adapters/db/AutoCommitPgProfileHistoryRepository.h"

#include "adapters/db/PgProfileHistoryRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

void AutoCommitPgProfileHistoryRepository::record(long long userId, const std::string& field,
                                                  const std::optional<std::string>& oldValue,
                                                  const std::optional<std::string>& newValue) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgProfileHistoryRepository(tx).record(userId, field, oldValue, newValue);
    tx.commit();
}

}  // namespace kinetiq::db
