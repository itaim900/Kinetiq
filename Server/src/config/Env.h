#pragma once

#include <cstdlib>
#include <string>

namespace fitapp {

// Returns the value of environment variable `key`, or `fallback` if it is
// unset or empty. Kept header-only and pure so it is trivially unit-testable.
inline std::string envOr(const char* key, const std::string& fallback) {
    const char* value = std::getenv(key);
    return (value != nullptr && *value != '\0') ? std::string(value) : fallback;
}

}  // namespace fitapp
