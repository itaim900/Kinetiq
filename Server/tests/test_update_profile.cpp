// Slice 2 (#19): UpdateProfile -- load-modify-save. Applies only the present
// fields of a ProfileUpdate onto the current user, validates the result, then
// persists. Tested against the in-memory fake.

#include <gtest/gtest.h>

#include "app/UpdateProfile.h"
#include "domain/ProfilePolicy.h"
#include "domain/User.h"
#include "support/InMemoryUserRepository.h"

using kinetiq::app::ProfileUpdate;
using kinetiq::app::ProfileUserNotFoundError;
using kinetiq::app::UpdateProfile;
using kinetiq::domain::ProfileValidationError;
using kinetiq::domain::User;
using kinetiq::testsupport::InMemoryUserRepository;

namespace {

// Seed a user with a known display name + one preset field, return its id.
long long seedUser(InMemoryUserRepository& repo) {
    User u;
    u.email = "seed@example.com";
    u.passwordHash = "x";
    u.displayName = "Seed";
    u.experienceLevel = "beginner";
    return repo.save(u).id;
}

}  // namespace

TEST(UpdateProfile, PartialUpdate_ChangesOnlyProvidedFields) {
    InMemoryUserRepository repo;
    const long long id = seedUser(repo);

    ProfileUpdate update;
    update.weightKg = 72.5;  // only weight provided

    UpdateProfile useCase(repo);
    const User result = useCase.execute(id, update);

    EXPECT_EQ(result.weightKg, 72.5);
    EXPECT_EQ(result.experienceLevel, "beginner");  // untouched
    EXPECT_EQ(result.displayName, "Seed");          // untouched

    const auto persisted = repo.findById(id);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_EQ(persisted->weightKg, 72.5);
    EXPECT_EQ(persisted->experienceLevel, "beginner");
}

TEST(UpdateProfile, InvalidValue_Throws_AndPersistsNothing) {
    InMemoryUserRepository repo;
    const long long id = seedUser(repo);

    ProfileUpdate update;
    update.weightKg = -5.0;  // invalid

    UpdateProfile useCase(repo);
    EXPECT_THROW(useCase.execute(id, update), ProfileValidationError);

    const auto persisted = repo.findById(id);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_FALSE(persisted->weightKg.has_value());  // unchanged
}

TEST(UpdateProfile, UnknownUser_ThrowsProfileUserNotFound) {
    InMemoryUserRepository repo;
    UpdateProfile useCase(repo);
    ProfileUpdate update;
    update.weightKg = 70.0;
    EXPECT_THROW(useCase.execute(999999, update), ProfileUserNotFoundError);
}

TEST(UpdateProfile, ClearField_EmptiesIt) {
    InMemoryUserRepository repo;
    const long long id = seedUser(repo);  // experience_level = "beginner"

    ProfileUpdate update;
    update.clear.insert("experience_level");
    UpdateProfile useCase(repo);
    const User result = useCase.execute(id, update);

    EXPECT_FALSE(result.experienceLevel.has_value());
    EXPECT_FALSE(repo.findById(id)->experienceLevel.has_value());  // persisted
}

TEST(UpdateProfile, ClearUnitPreference_Throws_OnThatField) {
    InMemoryUserRepository repo;
    const long long id = seedUser(repo);
    ProfileUpdate update;
    update.clear.insert("unit_preference");
    UpdateProfile useCase(repo);
    try {
        useCase.execute(id, update);
        FAIL() << "expected ProfileValidationError";
    } catch (const ProfileValidationError& e) {
        EXPECT_EQ(e.field(), "unit_preference");
    }
}
