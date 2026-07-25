#pragma once

#include "domain/Injury.h"

#include <optional>
#include <vector>

namespace kinetiq::ports {

// Repository port for User_Injuries persistence. Implementations:
// InMemoryInjuryRepository (tests/support/, fake) and PgInjuryRepository
// (adapters/db/, real) -- both satisfy this exact contract.
class IInjuryRepository {
public:
    virtual ~IInjuryRepository() = default;

    // Assigns an id (and created_at) and persists. Returns the stored injury.
    virtual domain::Injury create(const domain::Injury& injury) = 0;

    // All injuries belonging to userId (any active state), oldest first.
    virtual std::vector<domain::Injury> listByUser(long long userId) = 0;

    // Looks up by id regardless of owner (the caller checks ownership).
    virtual std::optional<domain::Injury> findById(long long id) = 0;

    // Overwrites the mutable fields (body_region, severity, active, notes) of
    // an existing injury matched by injury.id.
    virtual void update(const domain::Injury& injury) = 0;

    // Removes the injury with this id (no-op if it doesn't exist).
    virtual void deleteById(long long id) = 0;
};

}  // namespace kinetiq::ports
