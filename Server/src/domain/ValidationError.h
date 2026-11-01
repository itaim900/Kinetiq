#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace kinetiq::domain {

// Base of every "this field is invalid" error. Carries the field's wire name
// (e.g. "weight_kg") so the HTTP layer can build the RFC 9457 `errors` map.
// Subclasses (InjuryValidationError, ProfileValidationError, WeakPasswordError)
// only add a name that says where the error came from.
class ValidationError : public std::runtime_error {
public:
    ValidationError(std::string field, const std::string& message)
        : std::runtime_error(message), field_(std::move(field)) {}
    const std::string& field() const { return field_; }

private:
    std::string field_;
};

}  // namespace kinetiq::domain
