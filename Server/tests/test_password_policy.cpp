// Slice 1: password strength rule for registration. Minimum 8 characters plus
// at least one uppercase letter — decision recorded in docs/02-api-contract.md
// (updated 2026-07-12).

#include <gtest/gtest.h>

#include "domain/PasswordPolicy.h"

using kinetiq::domain::isPasswordStrong;

TEST(PasswordPolicy, RejectsPasswordsShorterThanEightChars) {
    EXPECT_FALSE(isPasswordStrong("Short1"));   // 6 chars, has uppercase, still too short
    EXPECT_FALSE(isPasswordStrong("Seven12"));  // 7 chars
}

TEST(PasswordPolicy, RejectsPasswordsWithNoUppercaseLetter) {
    EXPECT_FALSE(isPasswordStrong("alllowercase"));  // 12 chars, no uppercase
}

TEST(PasswordPolicy, AcceptsPasswordsWithEightCharsAndAnUppercaseLetter) {
    EXPECT_TRUE(isPasswordStrong("Abcdefgh"));           // exactly 8 chars, 1 uppercase
    EXPECT_TRUE(isPasswordStrong("A very long passphrase"));  // well over 8, uppercase at start
}

TEST(PasswordPolicy, RejectsEmptyPassword) {
    EXPECT_FALSE(isPasswordStrong(""));
}
