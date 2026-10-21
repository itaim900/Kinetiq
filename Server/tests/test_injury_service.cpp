// Slice 2 (#20): InjuryService -- CRUD over IInjuryRepository with an ownership
// check (another user's injury is invisible: throws InjuryNotFoundError).

#include <gtest/gtest.h>

#include "app/InjuryService.h"
#include "domain/InjuryPolicy.h"
#include "support/InMemoryInjuryRepository.h"

using kinetiq::app::AddInjuryRequest;
using kinetiq::app::InjuryNotFoundError;
using kinetiq::app::InjuryService;
using kinetiq::app::UpdateInjuryFields;
using kinetiq::domain::Injury;
using kinetiq::domain::InjuryValidationError;
using kinetiq::testsupport::InMemoryInjuryRepository;

namespace {

AddInjuryRequest addReq(long long userId, const std::string& region, const std::string& severity) {
    AddInjuryRequest r;
    r.userId = userId;
    r.bodyRegion = region;
    r.severity = severity;
    r.injuryType = "other";
    return r;
}

}  // namespace

TEST(InjuryService, Add_ValidInjury_CreatesAndAppearsInList) {
    InMemoryInjuryRepository repo;
    InjuryService svc(repo);

    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));
    EXPECT_GT(created.id, 0);

    const auto list = svc.list(1);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].bodyRegion, "lower_back");
}

TEST(InjuryService, Add_BadSeverity_Throws) {
    InMemoryInjuryRepository repo;
    InjuryService svc(repo);
    EXPECT_THROW(svc.add(addReq(1, "lower_back", "critical")), InjuryValidationError);
}

TEST(InjuryService, Update_OwnInjury_ChangesFields) {
    InMemoryInjuryRepository repo;
    InjuryService svc(repo);
    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));

    UpdateInjuryFields fields;
    fields.active = false;
    const Injury updated = svc.update(1, created.id, fields);
    EXPECT_FALSE(updated.active);
}

TEST(InjuryService, Update_OtherUsersInjury_ThrowsNotFound) {
    InMemoryInjuryRepository repo;
    InjuryService svc(repo);
    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));

    UpdateInjuryFields fields;
    fields.active = false;
    EXPECT_THROW(svc.update(2, created.id, fields), InjuryNotFoundError);  // user 2 != owner 1
}

TEST(InjuryService, Remove_OwnInjury_Deletes) {
    InMemoryInjuryRepository repo;
    InjuryService svc(repo);
    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));
    svc.remove(1, created.id);
    EXPECT_TRUE(svc.list(1).empty());
}

TEST(InjuryService, Remove_OtherUsersInjury_ThrowsNotFound) {
    InMemoryInjuryRepository repo;
    InjuryService svc(repo);
    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));
    EXPECT_THROW(svc.remove(2, created.id), InjuryNotFoundError);
    EXPECT_EQ(svc.list(1).size(), 1u);  // still there
}

TEST(InjuryService, Add_BadInjuryType_Throws) {
    InMemoryInjuryRepository repo;
    InjuryService svc(repo);
    auto req = addReq(1, "lower_back", "moderate");
    req.injuryType = "sprain";
    EXPECT_THROW(svc.add(req), InjuryValidationError);
}

TEST(InjuryService, Update_ChangesInjuryType) {
    InMemoryInjuryRepository repo;
    InjuryService svc(repo);
    const Injury created = svc.add(addReq(1, "shin", "mild"));

    UpdateInjuryFields fields;
    fields.injuryType = "bone";
    const Injury updated = svc.update(1, created.id, fields);
    EXPECT_EQ(updated.injuryType, "bone");
}
