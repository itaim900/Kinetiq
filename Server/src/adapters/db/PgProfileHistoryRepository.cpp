#include "adapters/db/PgProfileHistoryRepository.h"

namespace kinetiq::db {

void PgProfileHistoryRepository::record(long long userId, const std::string& field,
                                        const std::optional<std::string>& oldValue,
                                        const std::optional<std::string>& newValue) {
    // std::optional parameters become SQL NULL when empty.
    tx_.exec(
        "INSERT INTO user_profile_history (user_id, field, old_value, new_value) "
        "VALUES ($1, $2, $3, $4)",
        pqxx::params{userId, field, oldValue, newValue});
}

}  // namespace kinetiq::db
