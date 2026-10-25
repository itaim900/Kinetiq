#include "http/controllers/UserController.h"

#include "app/UpdateProfile.h"
#include "domain/ProfilePolicy.h"
#include "http/dto/AuthDto.h"      // dto::errorResponse (shared RFC 9457 helper)
#include "http/dto/ProfileDto.h"
#include "http/middleware/Authenticator.h"

namespace kinetiq::http {

void registerUserRoutes(crow::SimpleApp& app, kinetiq::ports::IUserRepository& users,
                        kinetiq::ports::IProfileHistoryRepository& history,
                        kinetiq::ports::ITokenService& tokens) {
    CROW_ROUTE(app, "/api/v1/users/me")
        .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Patch)(
            [&users, &history, &tokens](const crow::request& req) -> crow::response {
                const auto currentUser = authenticate(req, tokens);
                if (!currentUser.has_value()) {
                    return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                              "Missing, invalid, or expired token.");
                }

                if (req.method == crow::HTTPMethod::Get) {
                    const auto found = users.findById(currentUser->userId);
                    if (!found.has_value()) {
                        return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                                  "Missing, invalid, or expired token.");
                    }
                    return crow::response(200, dto::profileToJson(*found));
                }

                // PATCH
                const auto body = crow::json::load(req.body);
                if (!body) {
                    return dto::errorResponse(400, "malformed_request", "Malformed Request",
                                              "Request body is not valid JSON.");
                }

                kinetiq::app::UpdateProfile updateProfile(users, history);
                try {
                    const auto updated =
                        updateProfile.execute(currentUser->userId, dto::parseProfileUpdate(body));
                    return crow::response(200, dto::profileToJson(updated));
                } catch (const kinetiq::domain::ProfileValidationError& e) {
                    // Specific catch first: derives from std::runtime_error.
                    return dto::validationErrorResponse(e.field(), e.what());
                } catch (const kinetiq::app::ProfileUserNotFoundError&) {
                    return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                              "Missing, invalid, or expired token.");
                } catch (const std::exception&) {
                    // Wrong-typed JSON fields land here (e.g. weight as a string).
                    return dto::errorResponse(422, "validation_error", "Validation Failed",
                                              "One or more fields are missing or invalid.");
                }
            });
}

}  // namespace kinetiq::http
