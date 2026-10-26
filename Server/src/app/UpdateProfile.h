#pragma once

#include "domain/ProfilePolicy.h"
#include "domain/User.h"
#include "ports/IProfileHistoryRepository.h"
#include "ports/IUserRepository.h"

#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace kinetiq::app {

// Thrown when the authenticated user's id resolves to no user (defensive --
// shouldn't happen for a valid token, but handled rather than dereferencing an
// empty optional). The HTTP layer maps this to 401, same as GET /auth/me does
// for a valid-but-nonexistent user.
class ProfileUserNotFoundError : public std::runtime_error {
public:
    ProfileUserNotFoundError() : std::runtime_error("authenticated user not found") {}
};

// A partial profile update, following JSON Merge Patch (RFC 7396, ADR-0017):
// absent = unchanged, a value = set it, null = clear it (listed in `clear`).
// Supersedes Slice 2's D3 ("no null-clearing in v1").
struct ProfileUpdate {
    std::optional<std::string> sex;
    std::optional<std::string> birthDate;
    std::optional<double> heightCm;
    std::optional<double> weightKg;
    std::optional<std::string> experienceLevel;
    std::optional<std::string> unitPreference;
    std::optional<int> weeklyWorkoutGoal;
    std::optional<int> availableTimeMinutes;
    std::optional<std::string> medications;
    // Wire names of fields sent as JSON null -> clear them.
    std::set<std::string> clear;
};

namespace detail {
inline std::optional<std::string> asText(const std::optional<std::string>& v) { return v; }
inline std::optional<std::string> asText(const std::optional<double>& v) {
    if (!v) return std::nullopt;
    std::ostringstream os;
    os << *v;  // 72.5 -> "72.5" (std::to_string would give "72.500000")
    return os.str();
}
inline std::optional<std::string> asText(const std::optional<int>& v) {
    return v ? std::optional<std::string>(std::to_string(*v)) : std::nullopt;
}
// The 8 tracked fields, keyed by wire name (unit_preference deliberately excluded).
inline std::map<std::string, std::optional<std::string>> trackedValues(
    const kinetiq::domain::User& u) {
    return {{"sex", asText(u.sex)},
            {"birth_date", asText(u.birthDate)},
            {"height_cm", asText(u.heightCm)},
            {"weight_kg", asText(u.weightKg)},
            {"experience_level", asText(u.experienceLevel)},
            {"weekly_workout_goal", asText(u.weeklyWorkoutGoal)},
            {"available_time_minutes", asText(u.availableTimeMinutes)},
            {"medications", asText(u.medications)}};
}
}  // namespace detail

// Use case: load the current user, apply the provided fields, validate the
// resulting profile, persist, then record each changed field in the history.
// Depends only on ports.
class UpdateProfile {
public:
    UpdateProfile(kinetiq::ports::IUserRepository& users,
                  kinetiq::ports::IProfileHistoryRepository& history)
        : users_(users), history_(history) {}

    kinetiq::domain::User execute(long long userId, const ProfileUpdate& update) {
        auto found = users_.findById(userId);
        if (!found.has_value()) {
            throw ProfileUserNotFoundError();
        }

        kinetiq::domain::User user = *found;
        const auto before = detail::trackedValues(user);
        if (update.clear.count("unit_preference")) {
            throw kinetiq::domain::ProfileValidationError(
                "unit_preference", "unit_preference can't be cleared (it must be metric or imperial)");
        }
        if (update.clear.count("sex")) user.sex.reset();
        if (update.clear.count("birth_date")) user.birthDate.reset();
        if (update.clear.count("height_cm")) user.heightCm.reset();
        if (update.clear.count("weight_kg")) user.weightKg.reset();
        if (update.clear.count("experience_level")) user.experienceLevel.reset();
        if (update.clear.count("weekly_workout_goal")) user.weeklyWorkoutGoal.reset();
        if (update.clear.count("available_time_minutes")) user.availableTimeMinutes.reset();
        if (update.clear.count("medications")) user.medications.reset();
        if (update.sex) user.sex = update.sex;
        if (update.birthDate) user.birthDate = update.birthDate;
        if (update.heightCm) user.heightCm = update.heightCm;
        if (update.weightKg) user.weightKg = update.weightKg;
        if (update.experienceLevel) user.experienceLevel = update.experienceLevel;
        if (update.unitPreference) user.unitPreference = *update.unitPreference;
        if (update.weeklyWorkoutGoal) user.weeklyWorkoutGoal = update.weeklyWorkoutGoal;
        if (update.availableTimeMinutes) user.availableTimeMinutes = update.availableTimeMinutes;
        if (update.medications) user.medications = update.medications;

        // Guard: reject an invalid resulting profile BEFORE persisting.
        kinetiq::domain::requireValidProfile(user);

        users_.update(user);

        // Record each tracked field that actually changed (ADR-0017). Not
        // atomic with the save above (ADR-0015).
        for (const auto& [field, after] : detail::trackedValues(user)) {
            const auto& old = before.at(field);
            if (old != after) history_.record(userId, field, old, after);
        }
        return user;
    }

private:
    kinetiq::ports::IUserRepository& users_;
    kinetiq::ports::IProfileHistoryRepository& history_;
};

}  // namespace kinetiq::app
