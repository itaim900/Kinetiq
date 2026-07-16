#pragma once

#include "ports/IPasswordHasher.h"
#include "ports/ITokenService.h"
#include "ports/IUserRepository.h"

#include "crow_all.h"

namespace kinetiq::http {

// Registers auth routes (starting with POST /api/v1/auth/register) on
// `app`. A free function, not a class -- matching Authenticator.h's
// pattern -- so routes stay testable via HttpClient with no Crow-specific
// test scaffolding. Depends only on ports, so tests inject fakes and
// main.cpp injects the real adapters.
void registerAuthRoutes(crow::SimpleApp& app, kinetiq::ports::IUserRepository& users,
                         kinetiq::ports::IPasswordHasher& hasher,
                         kinetiq::ports::ITokenService& tokens);

}  // namespace kinetiq::http
