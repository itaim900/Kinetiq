#pragma once

#include <optional>
#include <string>

namespace kinetiq::domain {

// The user aggregate. Slice 1 needed only the auth subset; Slice 2 adds the
// profile fields. All profile fields are optional (an unset field is NULL in
// the DB -- the user is never forced to provide biometrics), except
// unitPreference, which the schema stores NOT NULL DEFAULT 'metric'.
struct User {
    long long id = 0;
    std::string email;
    std::string passwordHash;
    std::string displayName;

    // --- profile (Slice 2) ---
    std::optional<std::string> sex;              // male|female|other|unspecified
    std::optional<std::string> birthDate;        // ISO YYYY-MM-DD (age derived at read time)
    std::optional<double> heightCm;
    std::optional<double> weightKg;
    std::optional<std::string> experienceLevel;  // beginner|intermediate|advanced
    std::string unitPreference = "metric";       // metric|imperial (NOT NULL in DB)
    std::optional<int> weeklyWorkoutGoal;
    std::optional<int> availableTimeMinutes;
    std::optional<std::string> medications;      // free text in v1
};

}  // namespace kinetiq::domain
