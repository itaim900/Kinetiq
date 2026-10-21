// Injury conditions catalog: ICatalogRepository contract, exercised against the
// in-memory fake. PgCatalogRepository (Task 2) satisfies the same contract.

#include <gtest/gtest.h>

#include "support/InMemoryCatalogRepository.h"

using kinetiq::testsupport::InMemoryCatalogRepository;

TEST(InMemoryCatalogRepositoryTest, ListBodyRegions_ReturnsSeeded) {
    InMemoryCatalogRepository catalog;
    EXPECT_EQ(catalog.listBodyRegions().size(), 3u);
}

TEST(InMemoryCatalogRepositoryTest, BodyRegionExists) {
    InMemoryCatalogRepository catalog;
    EXPECT_TRUE(catalog.bodyRegionExists("knee"));
    EXPECT_FALSE(catalog.bodyRegionExists("left_knee"));
}

TEST(InMemoryCatalogRepositoryTest, FindMuscle) {
    InMemoryCatalogRepository catalog;
    const auto m = catalog.findMuscle(InMemoryCatalogRepository::kHamstrings);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->name, "Hamstrings");
    EXPECT_FALSE(catalog.findMuscle(999).has_value());
}

TEST(InMemoryCatalogRepositoryTest, FindCondition_ReturnsMusclesWithRoles) {
    InMemoryCatalogRepository catalog;
    const auto c = catalog.findCondition(InMemoryCatalogRepository::kHamstringStrain);
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->bodyRegion, "thigh");
    EXPECT_EQ(c->injuryType, "muscle");
    EXPECT_EQ(c->icd11Code, "NC76.30");
    ASSERT_EQ(c->muscles.size(), 2u);
    EXPECT_EQ(c->muscles[0].name, "Hamstrings");
    EXPECT_EQ(c->muscles[0].role, "primary");
    EXPECT_EQ(c->muscles[1].role, "secondary");
}

TEST(InMemoryCatalogRepositoryTest, FindCondition_Unknown_ReturnsNullopt) {
    InMemoryCatalogRepository catalog;
    EXPECT_FALSE(catalog.findCondition(999).has_value());
    EXPECT_EQ(catalog.listConditions().size(), 1u);
}
