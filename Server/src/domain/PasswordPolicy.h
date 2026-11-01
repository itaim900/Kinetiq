#pragma once

#include "domain/ValidationError.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>

namespace kinetiq::domain {

// Password rule: 8-64 characters, at least one uppercase letter. 64 follows
// NIST SP 800-63B ("allow at least 64") so password managers and passphrases
// fit; the cap only exists to stop absurdly long inputs. Recorded in
// docs/02-api-contract.md and spec 2026-11-01.
constexpr std::size_t kMinPasswordLength = 8;
constexpr std::size_t kMaxPasswordLength = 64;

namespace detail {

// Characters, not bytes: in UTF-8 every byte of a character except the first
// looks like 10xxxxxx in binary, so count the bytes that don't. ("א" is 2
// bytes but 1 character.)
inline std::size_t characterCount(const std::string& s) {
    return static_cast<std::size_t>(std::count_if(
        s.begin(), s.end(), [](unsigned char c) { return (c & 0xC0) != 0x80; }));
}

}  // namespace detail

inline bool isPasswordStrong(const std::string& password) {
    const std::size_t length = detail::characterCount(password);
    if (length < kMinPasswordLength || length > kMaxPasswordLength) return false;

    // unsigned char: passing a negative char to isupper is undefined behaviour.
    return std::any_of(password.begin(), password.end(),
                       [](unsigned char c) { return std::isupper(c); });
}

// Thrown by requireStrongPassword(). A ValidationError, so it carries the
// field name ("password" on register, "new_password" on change password).
class WeakPasswordError : public ValidationError {
public:
    using ValidationError::ValidationError;
};

// Throws WeakPasswordError (naming `field`) if `password` fails the policy;
// does nothing otherwise. Too long gets its own message so the user knows
// which way they're off.
inline void requireStrongPassword(const std::string& password,
                                  const std::string& field = "password") {
    if (isPasswordStrong(password)) return;

    if (detail::characterCount(password) > kMaxPasswordLength) {
        throw WeakPasswordError(field, field + " must be at most " +
                                           std::to_string(kMaxPasswordLength) + " characters");
    }
    throw WeakPasswordError(field, field + " must be at least " +
                                       std::to_string(kMinPasswordLength) +
                                       " characters and include an uppercase letter");
}

}  // namespace kinetiq::domain
