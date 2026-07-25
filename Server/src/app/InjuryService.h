#pragma once

#include "domain/Injury.h"
#include "domain/InjuryPolicy.h"
#include "ports/IInjuryRepository.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace kinetiq::app {

// Thrown when an injury doesn't exist OR isn't owned by the requesting user.
// Deliberately one type for both -- the HTTP layer maps it to 404 so another
// user's injury is indistinguishable from a nonexistent one (no existence leak).
class InjuryNotFoundError : public std::runtime_error {
public:
    InjuryNotFoundError() : std::runtime_error("injury not found") {}
};

struct AddInjuryRequest {
    long long userId = 0;
    std::string bodyRegion;
    std::string severity;
    std::optional<std::string> notes;
};

// Partial update: present fields change, absent fields are left unchanged.
struct UpdateInjuryFields {
    std::optional<std::string> bodyRegion;
    std::optional<std::string> severity;
    std::optional<std::string> notes;
    std::optional<bool> active;
};

// Cohesive CRUD service for a user's injuries. One class (not four use-case
// classes) because the four operations share the ownership check (D5).
class InjuryService {
public:
    explicit InjuryService(kinetiq::ports::IInjuryRepository& injuries) : injuries_(injuries) {}

    kinetiq::domain::Injury add(const AddInjuryRequest& req) {
        kinetiq::domain::Injury injury;
        injury.userId = req.userId;
        injury.bodyRegion = req.bodyRegion;
        injury.severity = req.severity;
        injury.notes = req.notes;
        injury.active = true;
        kinetiq::domain::requireValidInjury(injury);  // guard before persist
        return injuries_.create(injury);
    }

    std::vector<kinetiq::domain::Injury> list(long long userId) {
        return injuries_.listByUser(userId);
    }

    kinetiq::domain::Injury update(long long userId, long long injuryId,
                                   const UpdateInjuryFields& fields) {
        kinetiq::domain::Injury injury = requireOwned(userId, injuryId);
        if (fields.bodyRegion) injury.bodyRegion = *fields.bodyRegion;
        if (fields.severity) injury.severity = *fields.severity;
        if (fields.notes) injury.notes = fields.notes;
        if (fields.active) injury.active = *fields.active;
        kinetiq::domain::requireValidInjury(injury);  // guard before persist
        injuries_.update(injury);
        return injury;
    }

    void remove(long long userId, long long injuryId) {
        requireOwned(userId, injuryId);
        injuries_.deleteById(injuryId);
    }

private:
    // Loads the injury and asserts the caller owns it, else throws
    // InjuryNotFoundError (missing and not-owned are treated identically).
    kinetiq::domain::Injury requireOwned(long long userId, long long injuryId) {
        auto found = injuries_.findById(injuryId);
        if (!found.has_value() || found->userId != userId) {
            throw InjuryNotFoundError();
        }
        return *found;
    }

    kinetiq::ports::IInjuryRepository& injuries_;
};

}  // namespace kinetiq::app
