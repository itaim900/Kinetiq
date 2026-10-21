// InjuryService: CRUD with an ownership check, plus the two injury shapes --
// KNOWN (condition_id; region/type/muscles come from the catalog and are
// locked) and UNKNOWN (user supplies region/type/muscle_ids).

#include <gtest/gtest.h>

#include "app/InjuryService.h"
#include "domain/InjuryPolicy.h"
#include "support/InMemoryCatalogRepository.h"
#include "support/InMemoryInjuryRepository.h"

using kinetiq::app::AddInjuryRequest;
using kinetiq::app::InjuryNotFoundError;
using kinetiq::app::InjuryService;
using kinetiq::app::UpdateInjuryFields;
using kinetiq::domain::Injury;
using kinetiq::domain::InjuryValidationError;
using kinetiq::testsupport::InMemoryCatalogRepository;
using kinetiq::testsupport::InMemoryInjuryRepository;

namespace {

struct Fixture {
    InMemoryInjuryRepository repo;
    InMemoryCatalogRepository catalog;
    InjuryService svc{repo, catalog};
};

AddInjuryRequest unknownReq(long long userId, const std::string& region,
                            const std::string& severity) {
    AddInjuryRequest r;
    r.userId = userId;
    r.bodyRegion = region;
    r.injuryType = "other";
    r.severity = severity;
    return r;
}

AddInjuryRequest knownReq(long long userId, long long conditionId) {
    AddInjuryRequest r;
    r.userId = userId;
    r.conditionId = conditionId;
    r.severity = "moderate";
    return r;
}

// Asserts f() throws InjuryValidationError naming `field`.
template <typename F>
void expectFieldError(F&& f, const std::string& field) {
    try {
        f();
        FAIL() << "expected InjuryValidationError on " << field;
    } catch (const InjuryValidationError& e) {
        EXPECT_EQ(e.field(), field);
    }
}

}  // namespace

// --- unknown injuries ---

TEST(InjuryService, Add_ValidInjury_CreatesAndAppearsInList) {
    Fixture f;
    const Injury created = f.svc.add(unknownReq(1, "lower_back", "moderate"));
    EXPECT_GT(created.id, 0);
    EXPECT_FALSE(created.condition.has_value());
    const auto list = f.svc.list(1);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].bodyRegion, "lower_back");
}

TEST(InjuryService, Add_BadSeverity_Throws) {
    Fixture f;
    expectFieldError([&] { f.svc.add(unknownReq(1, "lower_back", "critical")); }, "severity");
}

TEST(InjuryService, Add_BadInjuryType_Throws) {
    Fixture f;
    auto req = unknownReq(1, "lower_back", "moderate");
    req.injuryType = "sprain";
    expectFieldError([&] { f.svc.add(req); }, "injury_type");
}

TEST(InjuryService, Add_Unknown_MissingBodyRegion_ThrowsOnBodyRegion) {
    Fixture f;
    auto req = unknownReq(1, "lower_back", "moderate");
    req.bodyRegion.reset();
    expectFieldError([&] { f.svc.add(req); }, "body_region");
}

TEST(InjuryService, Add_Unknown_MissingInjuryType_ThrowsOnInjuryType) {
    Fixture f;
    auto req = unknownReq(1, "lower_back", "moderate");
    req.injuryType.reset();
    expectFieldError([&] { f.svc.add(req); }, "injury_type");
}

TEST(InjuryService, Add_Unknown_UnknownRegion_ThrowsOnBodyRegion) {
    Fixture f;
    expectFieldError([&] { f.svc.add(unknownReq(1, "left_knee", "mild")); }, "body_region");
}

TEST(InjuryService, Add_Unknown_WithMuscles_ResolvesNamesAsPrimary) {
    Fixture f;
    auto req = unknownReq(1, "thigh", "mild");
    req.muscleIds = std::vector<long long>{InMemoryCatalogRepository::kQuads};
    const Injury created = f.svc.add(req);
    ASSERT_EQ(created.muscles.size(), 1u);
    EXPECT_EQ(created.muscles[0].name, "Quads");
    EXPECT_EQ(created.muscles[0].role, "primary");
}

TEST(InjuryService, Add_Unknown_DuplicateMuscleIds_Deduplicated) {
    Fixture f;
    auto req = unknownReq(1, "thigh", "mild");
    req.muscleIds = std::vector<long long>{InMemoryCatalogRepository::kQuads,
                                           InMemoryCatalogRepository::kQuads,
                                           InMemoryCatalogRepository::kCalves};
    EXPECT_EQ(f.svc.add(req).muscles.size(), 2u);
}

TEST(InjuryService, Add_Unknown_UnknownMuscleId_ThrowsOnMuscleIds) {
    Fixture f;
    auto req = unknownReq(1, "thigh", "mild");
    req.muscleIds = std::vector<long long>{999};
    expectFieldError([&] { f.svc.add(req); }, "muscle_ids");
}

// --- known conditions ---

TEST(InjuryService, Add_Known_CopiesRegionTypeAndMuscles) {
    Fixture f;
    const Injury created = f.svc.add(knownReq(1, InMemoryCatalogRepository::kHamstringStrain));
    ASSERT_TRUE(created.condition.has_value());
    EXPECT_EQ(created.condition->name, "Hamstring strain");
    EXPECT_EQ(created.condition->icd11Code, "NC76.30");
    EXPECT_EQ(created.bodyRegion, "thigh");
    EXPECT_EQ(created.injuryType, "muscle");
    ASSERT_EQ(created.muscles.size(), 2u);
    EXPECT_EQ(created.muscles[0].role, "primary");
}

TEST(InjuryService, Add_Known_UnknownConditionId_ThrowsOnConditionId) {
    Fixture f;
    expectFieldError([&] { f.svc.add(knownReq(1, 999)); }, "condition_id");
}

TEST(InjuryService, Add_Known_WithLockedFields_ThrowsOnThatField) {
    Fixture f;
    auto withRegion = knownReq(1, InMemoryCatalogRepository::kHamstringStrain);
    withRegion.bodyRegion = "knee";
    expectFieldError([&] { f.svc.add(withRegion); }, "body_region");

    auto withType = knownReq(1, InMemoryCatalogRepository::kHamstringStrain);
    withType.injuryType = "bone";
    expectFieldError([&] { f.svc.add(withType); }, "injury_type");

    auto withMuscles = knownReq(1, InMemoryCatalogRepository::kHamstringStrain);
    withMuscles.muscleIds = std::vector<long long>{InMemoryCatalogRepository::kQuads};
    expectFieldError([&] { f.svc.add(withMuscles); }, "muscle_ids");
}

// --- update ---

TEST(InjuryService, Update_OwnInjury_ChangesFields) {
    Fixture f;
    const Injury created = f.svc.add(unknownReq(1, "lower_back", "moderate"));
    UpdateInjuryFields fields;
    fields.active = false;
    EXPECT_FALSE(f.svc.update(1, created.id, fields).active);
}

TEST(InjuryService, Update_ChangesInjuryType) {
    Fixture f;
    const Injury created = f.svc.add(unknownReq(1, "lower_back", "mild"));
    UpdateInjuryFields fields;
    fields.injuryType = "bone";
    EXPECT_EQ(f.svc.update(1, created.id, fields).injuryType, "bone");
}

TEST(InjuryService, Update_Unknown_ReplacesAndClearsMuscles) {
    Fixture f;
    auto req = unknownReq(1, "thigh", "mild");
    req.muscleIds = std::vector<long long>{InMemoryCatalogRepository::kQuads};
    const Injury created = f.svc.add(req);

    UpdateInjuryFields replace;
    replace.muscleIds = std::vector<long long>{InMemoryCatalogRepository::kCalves};
    const Injury replaced = f.svc.update(1, created.id, replace);
    ASSERT_EQ(replaced.muscles.size(), 1u);
    EXPECT_EQ(replaced.muscles[0].name, "Calves");

    UpdateInjuryFields clear;
    clear.muscleIds = std::vector<long long>{};
    EXPECT_TRUE(f.svc.update(1, created.id, clear).muscles.empty());
}

TEST(InjuryService, Update_Unknown_UnknownRegion_ThrowsOnBodyRegion) {
    Fixture f;
    const Injury created = f.svc.add(unknownReq(1, "lower_back", "mild"));
    UpdateInjuryFields fields;
    fields.bodyRegion = "left_knee";
    expectFieldError([&] { f.svc.update(1, created.id, fields); }, "body_region");
}

TEST(InjuryService, Update_Known_LockedField_ThrowsOnField) {
    Fixture f;
    const Injury created = f.svc.add(knownReq(1, InMemoryCatalogRepository::kHamstringStrain));
    UpdateInjuryFields fields;
    fields.injuryType = "bone";
    expectFieldError([&] { f.svc.update(1, created.id, fields); }, "injury_type");
}

TEST(InjuryService, Update_Known_SeverityAllowed) {
    Fixture f;
    const Injury created = f.svc.add(knownReq(1, InMemoryCatalogRepository::kHamstringStrain));
    UpdateInjuryFields fields;
    fields.severity = "severe";
    const Injury updated = f.svc.update(1, created.id, fields);
    EXPECT_EQ(updated.severity, "severe");
    EXPECT_EQ(updated.muscles.size(), 2u);  // catalog muscles untouched
}

// --- ownership ---

TEST(InjuryService, Update_OtherUsersInjury_ThrowsNotFound) {
    Fixture f;
    const Injury created = f.svc.add(unknownReq(1, "lower_back", "moderate"));
    UpdateInjuryFields fields;
    fields.active = false;
    EXPECT_THROW(f.svc.update(2, created.id, fields), InjuryNotFoundError);
}

TEST(InjuryService, Remove_OwnInjury_Deletes) {
    Fixture f;
    const Injury created = f.svc.add(unknownReq(1, "lower_back", "moderate"));
    f.svc.remove(1, created.id);
    EXPECT_TRUE(f.svc.list(1).empty());
}

TEST(InjuryService, Remove_OtherUsersInjury_ThrowsNotFound) {
    Fixture f;
    const Injury created = f.svc.add(unknownReq(1, "lower_back", "moderate"));
    EXPECT_THROW(f.svc.remove(2, created.id), InjuryNotFoundError);
    EXPECT_EQ(f.svc.list(1).size(), 1u);
}
