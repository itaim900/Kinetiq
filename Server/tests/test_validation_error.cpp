// Domain review fixes (spec 2026-11-01): every field error derives from one
// ValidationError base, so the HTTP layer can treat them all the same way.

#include <gtest/gtest.h>

#include "domain/InjuryPolicy.h"
#include "domain/PasswordPolicy.h"
#include "domain/ProfilePolicy.h"
#include "domain/ValidationError.h"

#include <functional>
#include <string>

using kinetiq::domain::InjuryValidationError;
using kinetiq::domain::ProfileValidationError;
using kinetiq::domain::ValidationError;
using kinetiq::domain::WeakPasswordError;

namespace {

// Returns the field of the ValidationError `thrower` raises, or "<not caught>".
std::string fieldCaughtAsBase(const std::function<void()>& thrower) {
    try {
        thrower();
    } catch (const ValidationError& e) {
        return e.field();
    }
    return "<not caught>";
}

}  // namespace

TEST(ValidationError, BaseCatchesInjuryAndProfileErrors) {
    EXPECT_EQ(fieldCaughtAsBase([] { throw InjuryValidationError("severity", "bad"); }), "severity");
    EXPECT_EQ(fieldCaughtAsBase([] { throw ProfileValidationError("weight_kg", "bad"); }),
              "weight_kg");
    EXPECT_EQ(fieldCaughtAsBase([] { throw WeakPasswordError("password", "bad"); }), "password");
}

TEST(ValidationError, KeepsTheMessage) {
    const ValidationError e("height_cm", "too tall");
    EXPECT_STREQ(e.what(), "too tall");
}
