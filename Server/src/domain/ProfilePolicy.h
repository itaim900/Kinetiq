#pragma once

#include "domain/User.h"
#include "domain/ValidationError.h"

#include <initializer_list>
#include <string>

namespace kinetiq::domain {

// Thrown when a profile field is invalid (field name + message come from the
// ValidationError base; `using` reuses its constructor).
class ProfileValidationError : public ValidationError {
public:
    using ValidationError::ValidationError;
};

// Realistic upper limits (spec 2026-11-01), mirrored by DB CHECKs (0007) and
// the client's Zod schema.
constexpr int kMaxWeeklyWorkouts = 21;   // at most 3 workouts a day x 7 days
constexpr int kMaxSessionMinutes = 300;  // 5 hours

namespace detail {

inline bool isOneOf(const std::string& v, std::initializer_list<const char*> allowed) {
    for (const char* a : allowed) {
        if (v == a) return true;
    }
    return false;
}

// Gregorian rule: every 4th year, except centuries, except every 400th year
// (so 2000 was a leap year and 1900 wasn't).
inline bool isLeapYear(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }

inline int daysInMonth(int y, int m) {
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && isLeapYear(y)) ? 29 : kDays[m - 1];
}

// A real date in strict YYYY-MM-DD: digits only (no signs or spaces), years
// 1900-2100, and a day that exists in that month. Deliberately does NOT check
// "not in the future" against the wall clock -- that would make the validator
// (and its tests) time-dependent; a documented deferral.
inline bool isRealCalendarDate(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (int i : {0, 1, 2, 3, 5, 6, 8, 9}) {
        if (s[i] < '0' || s[i] > '9') return false;
    }
    // Digits -> number, e.g. "2001" -> 2001 ('7' - '0' == 7).
    const auto number = [&s](int from, int length) {
        int value = 0;
        for (int i = from; i < from + length; ++i) value = value * 10 + (s[i] - '0');
        return value;
    };
    const int y = number(0, 4), m = number(5, 2), d = number(8, 2);
    if (y < 1900 || y > 2100 || m < 1 || m > 12) return false;
    return d >= 1 && d <= daysInMonth(y, m);
}

}  // namespace detail

// Validates every provided profile field on `u`. Throws ProfileValidationError
// on the first invalid field; returns normally if all present fields are valid.
// Unset optionals are skipped (an absent field is always valid).
inline void requireValidProfile(const User& u) {
    if (u.weightKg && (*u.weightKg <= 0.0 || *u.weightKg > 500.0)) {
        throw ProfileValidationError("weight_kg", "weight_kg must be between 0 and 500");
    }
    if (u.heightCm && (*u.heightCm <= 0.0 || *u.heightCm > 300.0)) {
        throw ProfileValidationError("height_cm", "height_cm must be between 0 and 300");
    }
    if (u.experienceLevel &&
        !detail::isOneOf(*u.experienceLevel, {"beginner", "intermediate", "advanced"})) {
        throw ProfileValidationError("experience_level",
                                     "experience_level must be beginner, intermediate, or advanced");
    }
    if (u.sex && !detail::isOneOf(*u.sex, {"male", "female", "other", "unspecified"})) {
        throw ProfileValidationError("sex", "sex must be male, female, other, or unspecified");
    }
    if (!detail::isOneOf(u.unitPreference, {"metric", "imperial"})) {
        throw ProfileValidationError("unit_preference", "unit_preference must be metric or imperial");
    }
    if (u.weeklyWorkoutGoal &&
        (*u.weeklyWorkoutGoal < 0 || *u.weeklyWorkoutGoal > kMaxWeeklyWorkouts)) {
        throw ProfileValidationError("weekly_workout_goal",
                                     "weekly_workout_goal must be between 0 and " +
                                         std::to_string(kMaxWeeklyWorkouts));
    }
    if (u.availableTimeMinutes &&
        (*u.availableTimeMinutes < 1 || *u.availableTimeMinutes > kMaxSessionMinutes)) {
        throw ProfileValidationError("available_time_minutes",
                                     "available_time_minutes must be between 1 and " +
                                         std::to_string(kMaxSessionMinutes));
    }
    if (u.birthDate && !detail::isRealCalendarDate(*u.birthDate)) {
        throw ProfileValidationError("birth_date", "birth_date must be a valid YYYY-MM-DD date");
    }
}

}  // namespace kinetiq::domain
