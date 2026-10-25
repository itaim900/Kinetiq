#pragma once

#include <optional>
#include <string>

namespace kinetiq::ports {

// Append-only log of profile field changes (ADR-0017). Values are text so one
// table fits every field; nullopt = "not set". Implementations:
// InMemoryProfileHistoryRepository (tests/support/, fake) and
// PgProfileHistoryRepository (adapters/db/, real).
class IProfileHistoryRepository {
public:
    virtual ~IProfileHistoryRepository() = default;
    virtual void record(long long userId, const std::string& field,
                        const std::optional<std::string>& oldValue,
                        const std::optional<std::string>& newValue) = 0;
};

}  // namespace kinetiq::ports
