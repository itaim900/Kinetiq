#pragma once

#include <algorithm>
#include <cctype>
#include <stdexcept>
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

// Thrown by requireStrongPassword() -- used wherever the policy is enforced
// (RegisterUser now, ChangePassword later) so callers all fail the same way.
class WeakPasswordError : public std::runtime_error {
public:
    WeakPasswordError() : std::runtime_error("password does not meet the strength policy") {}
};

// Throws WeakPasswordError if `password` fails isPasswordStrong(); does
// nothing otherwise. A small helper so every caller doesn't repeat the same
// if-then-throw.
inline void requireStrongPassword(const std::string& password) {
    if (!isPasswordStrong(password)) {
        throw WeakPasswordError();
    }
}

}  // namespace fitapp::domain
