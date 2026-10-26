#pragma once

#include "ports/IInjuryRepository.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace kinetiq::testsupport {

// Hand-written in-memory fake for kinetiq::ports::IInjuryRepository. No gmock.
class InMemoryInjuryRepository : public kinetiq::ports::IInjuryRepository {
public:
    kinetiq::domain::Injury create(const kinetiq::domain::Injury& injury) override {
        kinetiq::domain::Injury stored = injury;
        stored.id = nextId_++;
        if (stored.createdAt.empty()) stored.createdAt = "1970-01-01T00:00:00Z";
        injuries_.push_back(stored);
        return stored;
    }

    std::vector<kinetiq::domain::Injury> listByUser(long long userId) override {
        std::vector<kinetiq::domain::Injury> out;
        for (const auto& i : injuries_) {
            if (i.userId == userId) out.push_back(i);
        }
        return out;
    }

    std::optional<kinetiq::domain::Injury> findById(long long id) override {
        for (const auto& i : injuries_) {
            if (i.id == id) return i;
        }
        return std::nullopt;
    }

    void update(const kinetiq::domain::Injury& injury) override {
        for (auto& i : injuries_) {
            if (i.id == injury.id) {
                i = injury;
                return;
            }
        }
    }

    void deleteById(long long id) override {
        injuries_.erase(std::remove_if(injuries_.begin(), injuries_.end(),
                                       [id](const kinetiq::domain::Injury& i) { return i.id == id; }),
                        injuries_.end());
    }

private:
    std::vector<kinetiq::domain::Injury> injuries_;
    long long nextId_ = 1;
};

}  // namespace kinetiq::testsupport
