#pragma once

#include <cctype>
#include <algorithm>
#include <string>

namespace fitapp::domain {

// Password strength rule: minimum 8 characters, at least one uppercase
// letter. Decision recorded in docs/02-api-contract.md (updated 2026-07-12).
inline bool isPasswordStrong(const std::string& password) {
    constexpr std::size_t kMinLength = 8;
    if (password.size() < kMinLength) return false;

    return std::any_of(password.begin(), password.end(), [](unsigned char c) {
        return std::isupper(c);
    });
}

}  // namespace fitapp::domain
