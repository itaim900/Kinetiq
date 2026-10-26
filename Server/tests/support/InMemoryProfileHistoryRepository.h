#pragma once

#include "ports/IProfileHistoryRepository.h"

#include <optional>
#include <string>
#include <vector>

namespace kinetiq::testsupport {

struct HistoryEntry {
    long long userId = 0;
    std::string field;
    std::optional<std::string> oldValue;
    std::optional<std::string> newValue;
};

// Fake: records entries in a public vector for tests to inspect. No gmock.
class InMemoryProfileHistoryRepository : public kinetiq::ports::IProfileHistoryRepository {
public:
    void record(long long userId, const std::string& field,
                const std::optional<std::string>& oldValue,
                const std::optional<std::string>& newValue) override {
        entries.push_back({userId, field, oldValue, newValue});
    }

    std::vector<HistoryEntry> entries;
};

}  // namespace kinetiq::testsupport
