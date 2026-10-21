// Slice 2 (#20): InjuryPolicy -- pure validation of an injury's fields,
// mirroring ProfilePolicy. requireValidInjury throws InjuryValidationError
// (carrying the offending field name) on the first invalid field.

#include <gtest/gtest.h>

#include "domain/Injury.h"
#include "domain/InjuryPolicy.h"

using kinetiq::domain::Injury;
using kinetiq::domain::InjuryValidationError;
using kinetiq::domain::requireValidInjury;

namespace {

Injury validInjury() {
    Injury i;
    i.userId = 1;
    i.bodyRegion = "lower_back";
    i.severity = "moderate";
    i.injuryType = "other";
    i.active = true;
    return i;
}

}  // namespace

TEST(InjuryPolicy, ValidInjury_DoesNotThrow) {
    EXPECT_NO_THROW(requireValidInjury(validInjury()));
}

TEST(InjuryPolicy, EmptyBodyRegion_Throws_FieldIsBodyRegion) {
    Injury i = validInjury();
    i.bodyRegion = "";
    try {
        requireValidInjury(i);
        FAIL() << "expected InjuryValidationError";
    } catch (const InjuryValidationError& e) {
        EXPECT_EQ(e.field(), "body_region");
    }
}

TEST(InjuryPolicy, BadSeverity_Throws_FieldIsSeverity) {
    Injury i = validInjury();
    i.severity = "critical";
    try {
        requireValidInjury(i);
        FAIL() << "expected InjuryValidationError";
    } catch (const InjuryValidationError& e) {
        EXPECT_EQ(e.field(), "severity");
    }
}

TEST(InjuryPolicy, EmptySeverity_Throws) {
    Injury i = validInjury();
    i.severity = "";
    EXPECT_THROW(requireValidInjury(i), InjuryValidationError);
}

TEST(InjuryPolicy, AllThreeSeverities_Accepted) {
    for (const char* s : {"mild", "moderate", "severe"}) {
        Injury i = validInjury();
        i.severity = s;
        EXPECT_NO_THROW(requireValidInjury(i)) << s;
    }
}

TEST(InjuryPolicy, BadInjuryType_Throws_FieldIsInjuryType) {
    Injury i = validInjury();
    i.injuryType = "sprain";  // not a real enum value
    try {
        requireValidInjury(i);
        FAIL() << "expected InjuryValidationError";
    } catch (const InjuryValidationError& e) {
        EXPECT_EQ(e.field(), "injury_type");
    }
}

TEST(InjuryPolicy, AllFiveInjuryTypes_Accepted) {
    for (const char* t : {"muscle", "bone", "tendon_ligament", "joint", "other"}) {
        Injury i = validInjury();
        i.injuryType = t;
        EXPECT_NO_THROW(requireValidInjury(i)) << t;
    }
}

TEST(InjuryPolicy, MuscleIdWithNonMuscleType_Throws_FieldIsMuscleId) {
    Injury i = validInjury();
    i.injuryType = "bone";
    i.muscleId = 7;
    try {
        requireValidInjury(i);
        FAIL() << "expected InjuryValidationError";
    } catch (const InjuryValidationError& e) {
        EXPECT_EQ(e.field(), "muscle_id");
    }
}

TEST(InjuryPolicy, MuscleIdWithMuscleType_DoesNotThrow) {
    Injury i = validInjury();
    i.injuryType = "muscle";
    i.muscleId = 7;
    EXPECT_NO_THROW(requireValidInjury(i));
}

TEST(InjuryPolicy, MuscleTypeWithoutMuscleId_DoesNotThrow) {
    Injury i = validInjury();
    i.injuryType = "muscle";  // muscleId left unset -- never required (E4)
    EXPECT_NO_THROW(requireValidInjury(i));
}
