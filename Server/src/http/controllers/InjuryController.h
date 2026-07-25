#pragma once

#include "ports/IInjuryRepository.h"
#include "ports/ITokenService.h"

#include "crow_all.h"

namespace kinetiq::http {

// Registers CRUD routes for /api/v1/users/me/injuries on `app`. Free function
// (matches AuthController/UserController). Depends only on ports.
void registerInjuryRoutes(crow::SimpleApp& app, kinetiq::ports::IInjuryRepository& injuries,
                          kinetiq::ports::ITokenService& tokens);

}  // namespace kinetiq::http
