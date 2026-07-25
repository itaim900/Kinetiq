#pragma once

#include "ports/ITokenService.h"
#include "ports/IUserRepository.h"

#include "crow_all.h"

namespace kinetiq::http {

// Registers GET/PATCH /api/v1/users/me on `app`. A free function (matches
// AuthController's pattern) so routes stay testable via HttpClient. Kept
// separate from AuthController: /auth/* is authentication, /users/* is profile.
void registerUserRoutes(crow::SimpleApp& app, kinetiq::ports::IUserRepository& users,
                        kinetiq::ports::ITokenService& tokens);

}  // namespace kinetiq::http
