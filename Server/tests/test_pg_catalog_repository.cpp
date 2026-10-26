// Injury conditions catalog: migration 0004 content + PgCatalogRepository
// against real Postgres (DbTest per-test rollback).

#include "support/TestDb.h"

#include <gtest/gtest.h>

#include "adapters/db/PgCatalogRepository.h"

#include <pqxx/pqxx>

using kinetiq::db::PgCatalogRepository;

namespace {
long long count(pqxx::work& tx, const std::string& sql) {
    return tx.exec(sql)[0][0].as<long long>();
}
long long conditionId(pqxx::work& tx, const std::string& slug) {
    return tx.exec("SELECT id FROM injury_conditions WHERE slug = $1", pqxx::params{slug})[0][0]
        .as<long long>();
}
}  // namespace

TEST_F(DbTest, Migration0004_SeedsCatalog) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    EXPECT_EQ(count(tx(), "SELECT count(*) FROM body_regions"), 14);
    EXPECT_EQ(count(tx(), "SELECT count(*) FROM muscles"), 28);
    EXPECT_EQ(count(tx(), "SELECT count(*) FROM injury_conditions"), 17);
    // 34 = sum of every condition's muscles in the spec table. A typo in a
    // muscle name would silently drop a row (INSERT ... JOIN), so pin the total.
    EXPECT_EQ(count(tx(), "SELECT count(*) FROM injury_condition_muscles"), 34);
}

TEST_F(DbTest, Migration0004_EveryConditionHasAMuscle) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    EXPECT_EQ(count(tx(),
                    "SELECT count(*) FROM injury_conditions c WHERE NOT EXISTS "
                    "(SELECT 1 FROM injury_condition_muscles cm WHERE cm.condition_id = c.id)"),
              0);
}

TEST_F(DbTest, Migration0004_RejectsUnknownBodyRegion) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    const long long userId =
        tx().exec("INSERT INTO users (email, password_hash) VALUES ('fk@example.com','x') RETURNING id")
            [0][0].as<long long>();
    EXPECT_THROW(tx().exec("INSERT INTO user_injuries (user_id, body_region, severity) "
                           "VALUES ($1, 'left_knee', 'mild')",
                           pqxx::params{userId}),
                 pqxx::sql_error);
}

TEST_F(DbTest, PgCatalogRepository_BodyRegions) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    PgCatalogRepository catalog(tx());
    EXPECT_EQ(catalog.listBodyRegions().size(), 14u);
    EXPECT_TRUE(catalog.bodyRegionExists("lower_back"));
    EXPECT_FALSE(catalog.bodyRegionExists("left_knee"));
}

TEST_F(DbTest, PgCatalogRepository_Muscles) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    PgCatalogRepository catalog(tx());
    const auto all = catalog.listMuscles();
    EXPECT_EQ(all.size(), 28u);
    const auto found = catalog.findMuscle(all[0].id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, all[0].name);
    EXPECT_FALSE(catalog.findMuscle(999999).has_value());
}

TEST_F(DbTest, PgCatalogRepository_FindCondition_AclTear) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    PgCatalogRepository catalog(tx());
    const auto c = catalog.findCondition(conditionId(tx(), "acl_tear"));
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->name, "ACL tear");
    EXPECT_EQ(c->icd11Code, "NC93.62");
    EXPECT_EQ(c->bodyRegion, "knee");
    EXPECT_EQ(c->injuryType, "tendon_ligament");
    ASSERT_EQ(c->muscles.size(), 3u);
    for (const auto& m : c->muscles) EXPECT_EQ(m.role, "secondary");
}

TEST_F(DbTest, PgCatalogRepository_FindCondition_PrimaryFirst) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    PgCatalogRepository catalog(tx());
    const auto c = catalog.findCondition(conditionId(tx(), "hamstring_strain"));
    ASSERT_TRUE(c.has_value());
    ASSERT_EQ(c->muscles.size(), 2u);
    EXPECT_EQ(c->muscles[0].name, "Hamstrings");
    EXPECT_EQ(c->muscles[0].role, "primary");
    EXPECT_EQ(c->muscles[1].name, "Glutes");
    EXPECT_EQ(c->muscles[1].role, "secondary");
}

TEST_F(DbTest, PgCatalogRepository_NullIcdCode_AndListConditions) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    PgCatalogRepository catalog(tx());
    const auto c = catalog.findCondition(conditionId(tx(), "shin_splints"));
    ASSERT_TRUE(c.has_value());
    EXPECT_FALSE(c->icd11Code.has_value());
    const auto all = catalog.listConditions();
    EXPECT_EQ(all.size(), 17u);
    for (const auto& cond : all) EXPECT_FALSE(cond.muscles.empty()) << cond.slug;
    EXPECT_FALSE(catalog.findCondition(999999).has_value());
}
