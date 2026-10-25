#pragma once

#include "ports/IProfileHistoryRepository.h"

#include <string>

namespace kinetiq::db {

// Production adapter: one connection + transaction per call (ADR-0015).
class AutoCommitPgProfileHistoryRepository : public kinetiq::ports::IProfileHistoryRepository {
public:
    explicit AutoCommitPgProfileHistoryRepository(std::string connString)
        : connString_(std::move(connString)) {}

    void record(long long userId, const std::string& field,
                const std::optional<std::string>& oldValue,
                const std::optional<std::string>& newValue) override;

private:
    std::string connString_;
};

}  // namespace kinetiq::db
