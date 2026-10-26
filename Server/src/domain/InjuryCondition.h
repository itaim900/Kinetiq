#pragma once

#include "domain/InjuryMuscle.h"

#include <optional>
#include <string>
#include <vector>

namespace kinetiq::domain {

// A known injury condition from the seeded catalog (e.g. "ACL tear"). It fixes
// the region, type and affected muscles of any injury that references it.
struct InjuryCondition {
    long long id = 0;
    std::string slug;
    std::string name;
    std::optional<std::string> icd11Code;  // WHO ICD-11; null when none found
    std::string bodyRegion;                // body_regions slug
    std::string injuryType;                // muscle|bone|tendon_ligament|joint|other
    std::vector<InjuryMuscle> muscles;     // primary first, then by name
};

}  // namespace kinetiq::domain
