#pragma once

#include <string>

namespace kinetiq::domain {

// One of the 14 fixed body regions (e.g. slug "lower_back", name "Lower back").
// Shared by injuries and exercise contraindications so the safety filter matches.
struct BodyRegion {
    std::string slug;
    std::string name;
};

}  // namespace kinetiq::domain
