#pragma once

#include <string>

namespace kinetiq::domain {

// A muscle as it relates to an injury. role: "primary" = the injured tissue
// itself, "secondary" = involved / to be protected.
struct InjuryMuscle {
    long long muscleId = 0;
    std::string name;
    std::string role;
};

}  // namespace kinetiq::domain
