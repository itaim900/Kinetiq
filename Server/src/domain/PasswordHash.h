#pragma once

#include <string>
#include <utility>

namespace kinetiq::domain {

// An encoded password hash (argon2's "$argon2id$v=19$m=...$salt$hash" string).
// A separate type rather than a std::string so the compiler rejects mixing it
// up with a plain password -- e.g. verify(password, hash) with the arguments
// swapped no longer compiles. The constructor is `explicit`, so a string never
// turns into a PasswordHash by accident; adapters unwrap it with encoded()
// only where they talk to argon2 or the database.
class PasswordHash {
public:
    PasswordHash() = default;
    explicit PasswordHash(std::string encoded) : encoded_(std::move(encoded)) {}

    const std::string& encoded() const { return encoded_; }
    bool operator==(const PasswordHash&) const = default;  // C++20 also gives !=

private:
    std::string encoded_;
};

}  // namespace kinetiq::domain
