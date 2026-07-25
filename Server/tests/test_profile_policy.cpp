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
