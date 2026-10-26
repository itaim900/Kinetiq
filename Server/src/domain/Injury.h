#pragma once

#include "domain/InjuryMuscle.h"

#include <optional>
#include <string>
#include <vector>

namespace kinetiq::domain {

// The catalog condition an injury refers to, as shown alongside the injury.
struct ConditionRef {
    long long id = 0;
    std::string name;
    std::optional<std::string> icd11Code;
};

// A user's injury. Either a KNOWN condition from the catalog (condition set:
// region, type and muscles come from the catalog and are locked) or an
// UNKNOWN injury the user described (condition empty: muscles are their picks).
// body_region drives the Phase 3 safety filter. Inactive injuries are kept for
// history but excluded from the filter.
struct Injury {
    long long id = 0;
    long long userId = 0;
    std::string bodyRegion;              // body_regions slug (e.g. "lower_back")
    std::string severity;                // mild|moderate|severe
    std::string injuryType;              // muscle|bone|tendon_ligament|joint|other
    std::optional<ConditionRef> condition;
    std::vector<InjuryMuscle> muscles;   // primary first, then by name
    bool active = true;
    std::optional<std::string> notes;
    std::string createdAt;               // ISO timestamp, set by the DB (read-only)
};

}  // namespace kinetiq::domain
