// Slice 1: password strength rule for registration. Minimum 8 characters plus
// at least one uppercase letter — decision recorded in docs/02-api-contract.md
// (updated 2026-07-12).

#include <gtest/gtest.h>

#include "domain/PasswordPolicy.h"

using kinetiq::domain::isPasswordStrong;
using kinetiq::domain::requireStrongPassword;
using kinetiq::domain::WeakPasswordError;

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

// Domain review fixes (spec 2026-11-01): 8-64 characters, counted as
// characters (not UTF-8 bytes); WeakPasswordError names the field.
TEST(PasswordPolicy, AcceptsUpTo64Characters_Rejects65) {
    EXPECT_TRUE(isPasswordStrong(std::string(63, 'a') + "A"));   // 64
    EXPECT_FALSE(isPasswordStrong(std::string(64, 'a') + "A"));  // 65
}

TEST(PasswordPolicy, CountsCharactersNotBytes) {
    std::string hebrew;
    for (int i = 0; i < 40; ++i) hebrew += "א";  // 2 bytes each in UTF-8
    EXPECT_TRUE(isPasswordStrong(hebrew + "A"));  // 41 characters (81 bytes)
}

TEST(PasswordPolicy, WeakPasswordErrorNamesTheField) {
    try {
        requireStrongPassword("weak", "new_password");
        FAIL() << "expected WeakPasswordError";
    } catch (const WeakPasswordError& e) {
        EXPECT_EQ(e.field(), "new_password");
        EXPECT_STREQ(e.what(),
                     "new_password must be at least 8 characters and include an uppercase letter");
    }
}

TEST(PasswordPolicy, TooLongPasswordHasItsOwnMessage) {
    try {
        requireStrongPassword(std::string(65, 'A'));
        FAIL() << "expected WeakPasswordError";
    } catch (const WeakPasswordError& e) {
        EXPECT_EQ(e.field(), "password");
        EXPECT_STREQ(e.what(), "password must be at most 64 characters");
    }
}
