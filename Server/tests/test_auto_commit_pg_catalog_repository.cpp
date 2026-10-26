// AutoCommitPgCatalogRepository -- the catalog wired into main.cpp. Read-only,
// so nothing to clean up.

#include "support/TestDb.h"

#include <gtest/gtest.h>

#include "adapters/db/AutoCommitPgCatalogRepository.h"

using kinetiq::db::AutoCommitPgCatalogRepository;

TEST(AutoCommitPgCatalogRepositoryTest, ReadsCatalogOverOwnConnections) {
    if (!kinetiq::testing::ensureTestDatabaseReady()) {
        GTEST_SKIP() << "no test database reachable";
    }
    AutoCommitPgCatalogRepository catalog(kinetiq::testing::testConnString());
    EXPECT_EQ(catalog.listBodyRegions().size(), 14u);
    EXPECT_TRUE(catalog.bodyRegionExists("knee"));
    EXPECT_EQ(catalog.listMuscles().size(), 28u);
    const auto conditions = catalog.listConditions();
    ASSERT_EQ(conditions.size(), 17u);
    const auto again = catalog.findCondition(conditions[0].id);
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(again->muscles.size(), conditions[0].muscles.size());
    EXPECT_TRUE(catalog.findMuscle(catalog.listMuscles()[0].id).has_value());
}
