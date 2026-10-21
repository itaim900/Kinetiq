#pragma once

#include "domain/Injury.h"

#include <stdexcept>
#include <string>

namespace kinetiq::domain {

// Thrown when an injury field is invalid. Carries the offending field's wire
// name (e.g. "severity") for the RFC 9457 errors object.
class InjuryValidationError : public std::runtime_error {
public:
    InjuryValidationError(std::string field, const std::string& message)
        : std::runtime_error(message), field_(std::move(field)) {}
    const std::string& field() const { return field_; }

private:
    std::string field_;
};

// Validates an injury's fields. Throws InjuryValidationError on the first
// invalid field; returns normally if valid.
inline void requireValidInjury(const Injury& injury) {
    if (injury.bodyRegion.empty()) {
        throw InjuryValidationError("body_region", "body_region must not be empty");
    }
    const bool severityOk = injury.severity == "mild" || injury.severity == "moderate" ||
                            injury.severity == "severe";
    if (!severityOk) {
        throw InjuryValidationError("severity", "severity must be mild, moderate, or severe");
    }
    const bool typeOk = injury.injuryType == "muscle" || injury.injuryType == "bone" ||
                        injury.injuryType == "tendon_ligament" || injury.injuryType == "joint" ||
                        injury.injuryType == "other";
    if (!typeOk) {
        throw InjuryValidationError(
            "injury_type", "injury_type must be muscle, bone, tendon_ligament, joint, or other");
    }
}

}  // namespace kinetiq::domain
