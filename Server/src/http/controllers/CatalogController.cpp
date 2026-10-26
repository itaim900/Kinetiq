#include "http/controllers/CatalogController.h"

#include "http/dto/AuthDto.h"  // dto::errorResponse
#include "http/dto/CatalogDto.h"
#include "http/middleware/Authenticator.h"

namespace kinetiq::http {

namespace {
crow::response unauthenticated() {
    return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                              "Missing, invalid, or expired token.");
}
}  // namespace

void registerCatalogRoutes(crow::SimpleApp& app, kinetiq::ports::ICatalogRepository& catalog,
                           kinetiq::ports::ITokenService& tokens) {
    CROW_ROUTE(app, "/api/v1/body-regions")
    ([&catalog, &tokens](const crow::request& req) -> crow::response {
        if (!authenticate(req, tokens)) return unauthenticated();
        return crow::response(200, dto::bodyRegionListToJson(catalog.listBodyRegions()));
    });

    CROW_ROUTE(app, "/api/v1/muscles")
    ([&catalog, &tokens](const crow::request& req) -> crow::response {
        if (!authenticate(req, tokens)) return unauthenticated();
        return crow::response(200, dto::muscleListToJson(catalog.listMuscles()));
    });

    CROW_ROUTE(app, "/api/v1/injury-conditions")
    ([&catalog, &tokens](const crow::request& req) -> crow::response {
        if (!authenticate(req, tokens)) return unauthenticated();
        return crow::response(200, dto::conditionListToJson(catalog.listConditions()));
    });
}

}  // namespace kinetiq::http
