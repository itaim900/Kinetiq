// Slice 2 (#19): ProfilePolicy -- pure, DB-free validation of profile fields,
// mirroring PasswordPolicy. requireValidProfile throws ProfileValidationError
// (carrying the offending field name) on the first invalid field.

#include <gtest/gtest.h>

#include "domain/ProfilePolicy.h"
#include "domain/User.h"

using kinetiq::domain::ProfileValidationError;
using kinetiq::domain::requireValidProfile;
using kinetiq::domain::User;

namespace {

// A fully-valid profile; individual tests corrupt one field.
User validProfile() {
    User u;
    u.email = "v@example.com";
    u.passwordHash = "x";
    u.sex = "female";
    u.birthDate = "1990-05-15";
    u.heightCm = 170.0;
    u.weightKg = 65.0;
    u.experienceLevel = "intermediate";
    u.unitPreference = "metric";
    u.weeklyWorkoutGoal = 4;
    u.availableTimeMinutes = 45;
    return u;
}

}  // namespace

TEST(ProfilePolicy, FullyValidProfile_DoesNotThrow) {
    EXPECT_NO_THROW(requireValidProfile(validProfile()));
}

TEST(ProfilePolicy, EmptyProfile_DoesNotThrow) {
    // All optionals unset, unitPreference at its default -- nothing to reject.
    User u;
    u.email = "e@example.com";
    u.passwordHash = "x";
    EXPECT_NO_THROW(requireValidProfile(u));
}

TEST(ProfilePolicy, ZeroWeight_Throws_FieldIsWeightKg) {
    User u = validProfile();
    u.weightKg = 0.0;
    try {
        requireValidProfile(u);
        FAIL() << "expected ProfileValidationError";
    } catch (const ProfileValidationError& e) {
        EXPECT_EQ(e.field(), "weight_kg");
    }
}

TEST(ProfilePolicy, NegativeWeight_Throws) {
    User u = validProfile();
    u.weightKg = -1.0;
    EXPECT_THROW(requireValidProfile(u), ProfileValidationError);
}

TEST(ProfilePolicy, ImplausiblyHighWeight_Throws) {
    User u = validProfile();
    u.weightKg = 501.0;
    EXPECT_THROW(requireValidProfile(u), ProfileValidationError);
}

TEST(ProfilePolicy, ImplausiblyHighHeight_Throws_FieldIsHeightCm) {
    User u = validProfile();
    u.heightCm = 300.1;
    try {
        requireValidProfile(u);
        FAIL() << "expected ProfileValidationError";
    } catch (const ProfileValidationError& e) {
        EXPECT_EQ(e.field(), "height_cm");
    }
}

TEST(ProfilePolicy, BadExperienceLevel_Throws) {
    User u = validProfile();
    u.experienceLevel = "expert";
    EXPECT_THROW(requireValidProfile(u), ProfileValidationError);
}

TEST(ProfilePolicy, BadSex_Throws) {
    User u = validProfile();
    u.sex = "yes";
    EXPECT_THROW(requireValidProfile(u), ProfileValidationError);
}

TEST(ProfilePolicy, BadUnitPreference_Throws) {
    User u = validProfile();
    u.unitPreference = "furlongs";
    EXPECT_THROW(requireValidProfile(u), ProfileValidationError);
}

TEST(ProfilePolicy, NegativeWeeklyGoal_Throws) {
    User u = validProfile();
    u.weeklyWorkoutGoal = -1;
    EXPECT_THROW(requireValidProfile(u), ProfileValidationError);
}

TEST(ProfilePolicy, ZeroAvailableTime_Throws) {
    User u = validProfile();
    u.availableTimeMinutes = 0;
    EXPECT_THROW(requireValidProfile(u), ProfileValidationError);
}

TEST(ProfilePolicy, MalformedBirthDate_Throws_FieldIsBirthDate) {
    User u = validProfile();
    u.birthDate = "May 5th 1990";
    try {
        requireValidProfile(u);
        FAIL() << "expected ProfileValidationError";
    } catch (const ProfileValidationError& e) {
        EXPECT_EQ(e.field(), "birth_date");
    }
}

// Domain review fixes (spec 2026-11-01): realistic upper limits and a real
// calendar check for birth dates.
namespace {

bool profileValidWith(void (*change)(User&)) {
    User u = validProfile();
    change(u);
    try {
        requireValidProfile(u);
        return true;
    } catch (const ProfileValidationError&) {
        return false;
    }
}

bool birthDateOk(const std::string& date) {
    User u = validProfile();
    u.birthDate = date;
    try {
        requireValidProfile(u);
        return true;
    } catch (const ProfileValidationError&) {
        return false;
    }
}

}  // namespace

TEST(ProfilePolicy, WeeklyGoal_0To21) {
    EXPECT_TRUE(profileValidWith([](User& u) { u.weeklyWorkoutGoal = 0; }));
    EXPECT_TRUE(profileValidWith([](User& u) { u.weeklyWorkoutGoal = 21; }));  // 3 a day x 7
    EXPECT_FALSE(profileValidWith([](User& u) { u.weeklyWorkoutGoal = 22; }));
}

TEST(ProfilePolicy, AvailableTime_1To300) {
    EXPECT_TRUE(profileValidWith([](User& u) { u.availableTimeMinutes = 1; }));
    EXPECT_TRUE(profileValidWith([](User& u) { u.availableTimeMinutes = 300; }));  // 5 hours
    EXPECT_FALSE(profileValidWith([](User& u) { u.availableTimeMinutes = 301; }));
}

TEST(ProfilePolicy, LimitMessagesNameTheRange) {
    User u = validProfile();
    u.weeklyWorkoutGoal = 22;
    try {
        requireValidProfile(u);
        FAIL() << "expected ProfileValidationError";
    } catch (const ProfileValidationError& e) {
        EXPECT_STREQ(e.what(), "weekly_workout_goal must be between 0 and 21");
    }

    u = validProfile();
    u.availableTimeMinutes = 301;
    try {
        requireValidProfile(u);
        FAIL() << "expected ProfileValidationError";
    } catch (const ProfileValidationError& e) {
        EXPECT_STREQ(e.what(), "available_time_minutes must be between 1 and 300");
    }
}

TEST(ProfilePolicy, BirthDate_MustBeARealCalendarDate) {
    EXPECT_TRUE(birthDateOk("2000-02-29"));   // divisible by 400 -> leap year
    EXPECT_TRUE(birthDateOk("2024-02-29"));   // divisible by 4 -> leap year
    EXPECT_TRUE(birthDateOk("2001-12-31"));
    EXPECT_FALSE(birthDateOk("1900-02-29"));  // century not divisible by 400 -> not leap
    EXPECT_FALSE(birthDateOk("2023-02-29"));
    EXPECT_FALSE(birthDateOk("2001-02-31"));
    EXPECT_FALSE(birthDateOk("2001-04-31"));  // April has 30 days
    EXPECT_FALSE(birthDateOk("2001-13-01"));
    EXPECT_FALSE(birthDateOk("2001-00-10"));
    EXPECT_FALSE(birthDateOk("2001-+1-05"));  // a sign sneaking in
    EXPECT_FALSE(birthDateOk("2001-1 -05"));  // a space sneaking in
    EXPECT_FALSE(birthDateOk("1899-12-31"));
}
