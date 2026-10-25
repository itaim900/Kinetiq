#pragma once

#include "domain/ProfilePolicy.h"
#include "domain/User.h"
#include "ports/IUserRepository.h"

#include <optional>
#include <set>
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

// Use case: load the current user, apply the provided fields, validate the
// resulting profile, persist. Depends only on the repository port.
class UpdateProfile {
public:
    explicit UpdateProfile(kinetiq::ports::IUserRepository& users) : users_(users) {}

    kinetiq::domain::User execute(long long userId, const ProfileUpdate& update) {
        auto found = users_.findById(userId);
        if (!found.has_value()) {
            throw ProfileUserNotFoundError();
        }

        kinetiq::domain::User user = *found;
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
        return user;
    }

private:
    kinetiq::ports::IUserRepository& users_;
};

}  // namespace kinetiq::app
