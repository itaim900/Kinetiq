#include "http/controllers/InjuryController.h"

#include "app/InjuryService.h"
#include "domain/InjuryPolicy.h"
#include "http/dto/AuthDto.h"      // dto::errorResponse
#include "http/dto/InjuryDto.h"
#include "http/dto/ProfileDto.h"   // dto::validationErrorResponse
#include "http/middleware/Authenticator.h"

#include <cstdint>

namespace kinetiq::http {

void registerInjuryRoutes(crow::SimpleApp& app, kinetiq::ports::IInjuryRepository& injuries,
                          kinetiq::ports::ICatalogRepository& catalog,
                          kinetiq::ports::ITokenService& tokens) {
    // Collection: GET (list) + POST (create).
    CROW_ROUTE(app, "/api/v1/users/me/injuries")
        .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)(
            [&injuries, &catalog, &tokens](const crow::request& req) -> crow::response {
                const auto currentUser = authenticate(req, tokens);
                if (!currentUser.has_value()) {
                    return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                              "Missing, invalid, or expired token.");
                }
                kinetiq::app::InjuryService service(injuries, catalog);

                if (req.method == crow::HTTPMethod::Get) {
                    return crow::response(200,
                                          dto::injuryListToJson(service.list(currentUser->userId)));
                }

                // POST
                const auto body = crow::json::load(req.body);
                if (!body) {
                    return dto::errorResponse(400, "malformed_request", "Malformed Request",
                                              "Request body is not valid JSON.");
                }
                try {
                    const auto created =
                        service.add(dto::parseAddInjury(body, currentUser->userId));
                    return crow::response(201, dto::injuryToJson(created));
                } catch (const kinetiq::domain::InjuryValidationError& e) {
                    return dto::validationErrorResponse(e.field(), e.what());
                } catch (const std::exception&) {
                    return dto::errorResponse(422, "validation_error", "Validation Failed",
                                              "One or more fields are missing or invalid.");
                }
            });

    // Item: PATCH + DELETE by id.
    CROW_ROUTE(app, "/api/v1/users/me/injuries/<int>")
        .methods(crow::HTTPMethod::Patch, crow::HTTPMethod::Delete)(
            [&injuries, &catalog, &tokens](const crow::request& req, std::int64_t id) -> crow::response {
                const auto currentUser = authenticate(req, tokens);
                if (!currentUser.has_value()) {
                    return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                              "Missing, invalid, or expired token.");
                }
                kinetiq::app::InjuryService service(injuries, catalog);

                try {
                    if (req.method == crow::HTTPMethod::Delete) {
                        service.remove(currentUser->userId, id);
                        return crow::response(204);
                    }
                    // PATCH
                    const auto body = crow::json::load(req.body);
                    if (!body) {
                        return dto::errorResponse(400, "malformed_request", "Malformed Request",
                                                  "Request body is not valid JSON.");
                    }
                    const auto updated =
                        service.update(currentUser->userId, id, dto::parseUpdateInjury(body));
                    return crow::response(200, dto::injuryToJson(updated));
                } catch (const kinetiq::app::InjuryNotFoundError&) {
                    return dto::errorResponse(404, "not_found", "Not Found", "Injury not found.");
                } catch (const kinetiq::domain::InjuryValidationError& e) {
                    return dto::validationErrorResponse(e.field(), e.what());
                } catch (const std::exception&) {
                    return dto::errorResponse(422, "validation_error", "Validation Failed",
                                              "One or more fields are missing or invalid.");
                }
            });
}

}  // namespace kinetiq::http
