#pragma once

#include <string>

namespace kinetiq::domain {

// A catalog muscle (e.g. "Hamstrings"). Read-only from the API's perspective --
// seeded by migrations, never created/edited by a client.
struct Muscle {
    long long id = 0;
    std::string name;
    std::string muscleGroup;
};

}  // namespace kinetiq::domain
