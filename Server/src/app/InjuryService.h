#pragma once

#include "domain/Injury.h"
#include "domain/InjuryPolicy.h"
#include "ports/ICatalogRepository.h"
#include "ports/IInjuryRepository.h"

#include <algorithm>
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

// Two shapes: KNOWN (conditionId set; bodyRegion/injuryType/muscleIds must be
// absent) or UNKNOWN (bodyRegion + injuryType required, muscleIds optional).
struct AddInjuryRequest {
    long long userId = 0;
    std::optional<long long> conditionId;
    std::optional<std::string> bodyRegion;
    std::optional<std::string> injuryType;
    std::optional<std::vector<long long>> muscleIds;
    std::string severity;
    std::optional<std::string> notes;
};

// Partial update: present fields change, absent fields are left unchanged.
// For a KNOWN injury, bodyRegion/injuryType/muscleIds are locked.
// muscleIds replaces the whole set (an empty list clears it).
struct UpdateInjuryFields {
    std::optional<std::string> bodyRegion;
    std::optional<std::string> severity;
    std::optional<std::string> injuryType;
    std::optional<std::vector<long long>> muscleIds;
    std::optional<std::string> notes;
    std::optional<bool> active;
};

// Cohesive CRUD service for a user's injuries (one class because the
// operations share the ownership check). Uses the catalog to resolve known
// conditions and to check that regions and muscles exist.
class InjuryService {
public:
    InjuryService(kinetiq::ports::IInjuryRepository& injuries,
                  kinetiq::ports::ICatalogRepository& catalog)
        : injuries_(injuries), catalog_(catalog) {}

    kinetiq::domain::Injury add(const AddInjuryRequest& req) {
        kinetiq::domain::Injury injury;
        injury.userId = req.userId;
        injury.severity = req.severity;
        injury.notes = req.notes;
        injury.active = true;

        if (req.conditionId) {
            rejectLocked(req.bodyRegion.has_value(), "body_region");
            rejectLocked(req.injuryType.has_value(), "injury_type");
            rejectLocked(req.muscleIds.has_value(), "muscle_ids");
            const auto condition = catalog_.findCondition(*req.conditionId);
            if (!condition) {
                throw kinetiq::domain::InjuryValidationError(
                    "condition_id", "condition_id does not match a known condition");
            }
            injury.condition =
                kinetiq::domain::ConditionRef{condition->id, condition->name, condition->icd11Code};
            injury.bodyRegion = condition->bodyRegion;
            injury.injuryType = condition->injuryType;
            injury.muscles = condition->muscles;
        } else {
            if (!req.bodyRegion) {
                throw kinetiq::domain::InjuryValidationError(
                    "body_region", "body_region is required unless condition_id is given");
            }
            if (!req.injuryType) {
                throw kinetiq::domain::InjuryValidationError(
                    "injury_type", "injury_type is required unless condition_id is given");
            }
            injury.bodyRegion = *req.bodyRegion;
            injury.injuryType = *req.injuryType;
            injury.muscles = resolveMuscles(req.muscleIds.value_or(std::vector<long long>{}));
        }

        kinetiq::domain::requireValidInjury(injury);
        if (!injury.condition) requireKnownRegion(injury.bodyRegion);
        return injuries_.create(injury);
    }

    std::vector<kinetiq::domain::Injury> list(long long userId) {
        return injuries_.listByUser(userId);
    }

    kinetiq::domain::Injury update(long long userId, long long injuryId,
                                   const UpdateInjuryFields& fields) {
        kinetiq::domain::Injury injury = requireOwned(userId, injuryId);
        if (injury.condition) {
            rejectLocked(fields.bodyRegion.has_value(), "body_region");
            rejectLocked(fields.injuryType.has_value(), "injury_type");
            rejectLocked(fields.muscleIds.has_value(), "muscle_ids");
        } else {
            if (fields.bodyRegion) injury.bodyRegion = *fields.bodyRegion;
            if (fields.injuryType) injury.injuryType = *fields.injuryType;
            if (fields.muscleIds) injury.muscles = resolveMuscles(*fields.muscleIds);
        }
        if (fields.severity) injury.severity = *fields.severity;
        if (fields.notes) injury.notes = fields.notes;
        if (fields.active) injury.active = *fields.active;

        kinetiq::domain::requireValidInjury(injury);
        if (!injury.condition) requireKnownRegion(injury.bodyRegion);
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

    // A known condition fixes this field; sending it is an error, not ignored.
    static void rejectLocked(bool present, const std::string& field) {
        if (present) {
            throw kinetiq::domain::InjuryValidationError(
                field, field + " cannot be set for a known condition (it comes from the catalog)");
        }
    }

    void requireKnownRegion(const std::string& slug) {
        if (!catalog_.bodyRegionExists(slug)) {
            throw kinetiq::domain::InjuryValidationError("body_region",
                                                         "body_region is not a known region");
        }
    }

    // Looks up each id (de-duplicated, order kept) and returns the muscles with
    // role "primary". Throws on the first id the catalog doesn't know.
    std::vector<kinetiq::domain::InjuryMuscle> resolveMuscles(const std::vector<long long>& ids) {
        std::vector<kinetiq::domain::InjuryMuscle> out;
        for (long long id : ids) {
            const bool seen = std::any_of(out.begin(), out.end(),
                                          [id](const auto& m) { return m.muscleId == id; });
            if (seen) continue;
            const auto muscle = catalog_.findMuscle(id);
            if (!muscle) {
                throw kinetiq::domain::InjuryValidationError(
                    "muscle_ids", "muscle_ids contains an unknown muscle id: " + std::to_string(id));
            }
            out.push_back({muscle->id, muscle->name, "primary"});
        }
        return out;
    }

    kinetiq::ports::IInjuryRepository& injuries_;
    kinetiq::ports::ICatalogRepository& catalog_;
};

}  // namespace kinetiq::app
