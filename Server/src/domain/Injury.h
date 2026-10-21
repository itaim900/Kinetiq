#pragma once

#include <optional>
#include <string>

namespace kinetiq::domain {

// A user's injury. Drives the Phase 3 rule-based safety filter (body_region is
// JOINed against exercise_contraindications) -- body_region is unaffected by
// the muscle link below. Inactive injuries are retained for history but
// excluded from the filter.
struct Injury {
    long long id = 0;
    long long userId = 0;
    std::string bodyRegion;             // free text in v1 (e.g. "lower_back")
    std::string severity;               // mild|moderate|severe
    std::string injuryType;             // muscle|bone|tendon_ligament|joint|other (required)
    std::optional<long long> muscleId;  // nullable; valid only if injuryType == "muscle"
    bool active = true;
    std::optional<std::string> notes;
    std::string createdAt;              // ISO timestamp, set by the DB (read-only)
};

}  // namespace kinetiq::domain
