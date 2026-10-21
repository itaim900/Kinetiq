#pragma once

#include "ports/ICatalogRepository.h"
#include "ports/ITokenService.h"

#include "crow_all.h"

namespace kinetiq::http {

// Registers the read-only reference-data routes: GET /api/v1/body-regions,
// /api/v1/muscles, /api/v1/injury-conditions. All require a valid token.
void registerCatalogRoutes(crow::SimpleApp& app, kinetiq::ports::ICatalogRepository& catalog,
                           kinetiq::ports::ITokenService& tokens);

}  // namespace kinetiq::http
