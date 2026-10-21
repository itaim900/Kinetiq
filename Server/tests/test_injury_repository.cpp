// Slice 2 (#20): IInjuryRepository, exercised against the in-memory fake. The
// real adapter (PgInjuryRepository) must satisfy the same contract.

#include <gtest/gtest.h>

#include "domain/Injury.h"
#include "support/InMemoryInjuryRepository.h"

using kinetiq::domain::Injury;
using kinetiq::testsupport::InMemoryInjuryRepository;

namespace {

Injury make(long long userId, const std::string& region, const std::string& severity) {
    Injury i;
    i.userId = userId;
    i.bodyRegion = region;
    i.severity = severity;
    i.injuryType = "other";
    return i;
}

}  // namespace

TEST(InMemoryInjuryRepositoryTest, Create_AssignsIdAndPersists) {
    InMemoryInjuryRepository repo;
    const Injury saved = repo.create(make(1, "lower_back", "moderate"));
    EXPECT_GT(saved.id, 0);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->bodyRegion, "lower_back");
    EXPECT_EQ(found->userId, 1);
}

TEST(InMemoryInjuryRepositoryTest, ListByUser_ReturnsOnlyThatUsersInjuries) {
    InMemoryInjuryRepository repo;
    repo.create(make(1, "lower_back", "mild"));
    repo.create(make(1, "left_knee", "moderate"));
    repo.create(make(2, "right_shoulder", "severe"));

    const auto forUser1 = repo.listByUser(1);
    EXPECT_EQ(forUser1.size(), 2u);
    for (const auto& i : forUser1) EXPECT_EQ(i.userId, 1);
}

TEST(InMemoryInjuryRepositoryTest, FindById_UnknownId_ReturnsNullopt) {
    InMemoryInjuryRepository repo;
    EXPECT_FALSE(repo.findById(999).has_value());
}

TEST(InMemoryInjuryRepositoryTest, Update_ChangesStoredFields) {
    InMemoryInjuryRepository repo;
    const Injury saved = repo.create(make(1, "lower_back", "moderate"));

    Injury modified = saved;
    modified.active = false;
    modified.severity = "mild";
    repo.update(modified);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_FALSE(found->active);
    EXPECT_EQ(found->severity, "mild");
}

TEST(InMemoryInjuryRepositoryTest, DeleteById_RemovesInjury) {
    InMemoryInjuryRepository repo;
    const Injury saved = repo.create(make(1, "lower_back", "moderate"));
    repo.deleteById(saved.id);
    EXPECT_FALSE(repo.findById(saved.id).has_value());
    EXPECT_TRUE(repo.listByUser(1).empty());
}

// condition/muscles round-trip with zero fake-code changes (stores by value).
TEST(InMemoryInjuryRepositoryTest, Create_PersistsConditionAndMuscles) {
    InMemoryInjuryRepository repo;
    Injury i = make(1, "thigh", "moderate");
    i.injuryType = "muscle";
    i.condition = kinetiq::domain::ConditionRef{4, "Hamstring strain", "NC76.30"};
    i.muscles = {{15, "Hamstrings", "primary"}};
    const Injury saved = repo.create(i);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    ASSERT_TRUE(found->condition.has_value());
    EXPECT_EQ(found->condition->name, "Hamstring strain");
    ASSERT_EQ(found->muscles.size(), 1u);
    EXPECT_EQ(found->muscles[0].name, "Hamstrings");
}
