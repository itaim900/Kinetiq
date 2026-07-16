#pragma once

#include "ports/IUserRepository.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <unordered_map>

namespace fitapp::testsupport {

// Hand-written in-memory fake for fitapp::ports::IUserRepository, used in
// unit/HTTP-in-process tests in place of PgUserRepository. No gmock -- see
// docs/phase-2-plan.md §8 (Testing architecture).
class InMemoryUserRepository : public fitapp::ports::IUserRepository {
public:
    fitapp::domain::User save(const fitapp::domain::User& user) override {
        const std::string key = toLower(user.email);
        if (byEmail_.contains(key)) {
            throw fitapp::ports::DuplicateEmailError(user.email);
        }
        fitapp::domain::User stored = user;
        stored.id = nextId_++;
        byEmail_.emplace(key, stored);
        return stored;
    }

    std::optional<fitapp::domain::User> findByEmail(const std::string& email) override {
        const auto it = byEmail_.find(toLower(email));
        if (it == byEmail_.end()) return std::nullopt;
        return it->second;
    }

    std::optional<fitapp::domain::User> findById(long long id) override {
        // No secondary index by id -- a linear scan is fine for a test
        // fake's tiny datasets. Correctness over cleverness here.
        for (const auto& [key, user] : byEmail_) {
            if (user.id == id) return user;
        }
        return std::nullopt;
    }

    void update(const fitapp::domain::User& user) override {
        for (auto it = byEmail_.begin(); it != byEmail_.end(); ++it) {
            if (it->second.id != user.id) continue;

            const std::string newKey = toLower(user.email);
            if (newKey == it->first) {
                it->second = user;
            } else {
                // Email changed -- the map is keyed by (lowercased) email,
                // so the entry has to move.
                byEmail_.erase(it);
                byEmail_.emplace(newKey, user);
            }
            return;
        }
    }

private:
    static std::string toLower(const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    std::unordered_map<std::string, fitapp::domain::User> byEmail_;
    long long nextId_ = 1;
};

}  // namespace fitapp::testsupport
