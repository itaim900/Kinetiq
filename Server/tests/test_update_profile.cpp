// Slice 2 (#19): UpdateProfile -- load-modify-save. Applies only the present
// fields of a ProfileUpdate onto the current user, validates the result, then
// persists. Tested against the in-memory fake.

#include <gtest/gtest.h>

#include "app/UpdateProfile.h"
#include "domain/ProfilePolicy.h"
#include "domain/User.h"
#include "support/InMemoryProfileHistoryRepository.h"
#include "support/InMemoryUserRepository.h"

using kinetiq::app::ProfileUpdate;
using kinetiq::app::ProfileUserNotFoundError;
using kinetiq::app::UpdateProfile;
using kinetiq::domain::ProfileValidationError;
using kinetiq::domain::User;
using kinetiq::testsupport::InMemoryProfileHistoryRepository;
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

    InMemoryProfileHistoryRepository history;
    UpdateProfile useCase(repo, history);
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

    InMemoryProfileHistoryRepository history;
    UpdateProfile useCase(repo, history);
    EXPECT_THROW(useCase.execute(id, update), ProfileValidationError);

    const auto persisted = repo.findById(id);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_FALSE(persisted->weightKg.has_value());  // unchanged
}

TEST(UpdateProfile, UnknownUser_ThrowsProfileUserNotFound) {
    InMemoryUserRepository repo;
    InMemoryProfileHistoryRepository history;
    UpdateProfile useCase(repo, history);
    ProfileUpdate update;
    update.weightKg = 70.0;
    EXPECT_THROW(useCase.execute(999999, update), ProfileUserNotFoundError);
}

TEST(UpdateProfile, ClearField_EmptiesIt) {
    InMemoryUserRepository repo;
    const long long id = seedUser(repo);  // experience_level = "beginner"

    ProfileUpdate update;
    update.clear.insert("experience_level");
    InMemoryProfileHistoryRepository history;
    UpdateProfile useCase(repo, history);
    const User result = useCase.execute(id, update);

    EXPECT_FALSE(result.experienceLevel.has_value());
    EXPECT_FALSE(repo.findById(id)->experienceLevel.has_value());  // persisted
}

TEST(UpdateProfile, ClearUnitPreference_Throws_OnThatField) {
    InMemoryUserRepository repo;
    const long long id = seedUser(repo);
    ProfileUpdate update;
    update.clear.insert("unit_preference");
    InMemoryProfileHistoryRepository history;
    UpdateProfile useCase(repo, history);
    try {
        useCase.execute(id, update);
        FAIL() << "expected ProfileValidationError";
    } catch (const ProfileValidationError& e) {
        EXPECT_EQ(e.field(), "unit_preference");
    }
}

TEST(UpdateProfile, RecordsOnlyChangedFields) {
    InMemoryUserRepository repo;
    InMemoryProfileHistoryRepository history;
    const long long id = seedUser(repo);  // experience_level = "beginner"

    ProfileUpdate update;
    update.weightKg = 72.5;
    update.experienceLevel = "beginner";  // same value -> no row
    UpdateProfile(repo, history).execute(id, update);

    ASSERT_EQ(history.entries.size(), 1u);
    EXPECT_EQ(history.entries[0].userId, id);
    EXPECT_EQ(history.entries[0].field, "weight_kg");
    EXPECT_FALSE(history.entries[0].oldValue.has_value());
    EXPECT_EQ(history.entries[0].newValue, "72.5");
}

TEST(UpdateProfile, ClearingRecordsTheOldValue) {
    InMemoryUserRepository repo;
    InMemoryProfileHistoryRepository history;
    const long long id = seedUser(repo);

    ProfileUpdate update;
    update.clear.insert("experience_level");
    UpdateProfile(repo, history).execute(id, update);

    ASSERT_EQ(history.entries.size(), 1u);
    EXPECT_EQ(history.entries[0].field, "experience_level");
    EXPECT_EQ(history.entries[0].oldValue, "beginner");
    EXPECT_FALSE(history.entries[0].newValue.has_value());
}

TEST(UpdateProfile, UnitPreferenceIsNotTracked) {
    InMemoryUserRepository repo;
    InMemoryProfileHistoryRepository history;
    const long long id = seedUser(repo);
    ProfileUpdate update;
    update.unitPreference = "imperial";
    UpdateProfile(repo, history).execute(id, update);
    EXPECT_TRUE(history.entries.empty());
}
