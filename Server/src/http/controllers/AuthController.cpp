#include "http/controllers/AuthController.h"

#include "app/ChangePassword.h"
#include "app/LoginUser.h"
#include "app/RegisterUser.h"
#include "http/dto/AuthDto.h"
#include "http/middleware/Authenticator.h"

namespace kinetiq::http {

void registerAuthRoutes(crow::SimpleApp& app, kinetiq::ports::IUserRepository& users,
                         kinetiq::ports::IPasswordHasher& hasher,
                         kinetiq::ports::ITokenService& tokens) {
    CROW_ROUTE(app, "/api/v1/auth/register")
        .methods(crow::HTTPMethod::Post)([&users, &hasher, &tokens](const crow::request& req) {
            const auto body = crow::json::load(req.body);
            if (!body) {
                return dto::errorResponse(400, "malformed_request", "Malformed Request",
                                           "Request body is not valid JSON.");
            }

            kinetiq::app::RegisterUser registerUser(users, hasher, tokens);
            try {
                const auto result = registerUser.execute(dto::parseRegisterRequest(body));
                auto json = dto::authResultToJson(result.user, result.accessToken);
                return crow::response(201, json);
            } catch (const kinetiq::domain::WeakPasswordError&) {
                return dto::errorResponse(422, "validation_error", "Validation Failed",
                                           "Password does not meet the strength policy.");
            } catch (const kinetiq::ports::DuplicateEmailError&) {
                return dto::errorResponse(409, "conflict", "Conflict",
                                           "Email is already registered.");
            } catch (const std::exception&) {
                // Catches missing/wrong-typed JSON fields (parseRegisterRequest
                // throws std::runtime_error for those) -- must come after the
                // two specific catches above, since both derive from
                // std::runtime_error and would otherwise be caught here first.
                return dto::errorResponse(422, "validation_error", "Validation Failed",
                                           "One or more required fields are missing or invalid.");
            }
        });

    CROW_ROUTE(app, "/api/v1/auth/login")
        .methods(crow::HTTPMethod::Post)([&users, &hasher, &tokens](const crow::request& req) {
            const auto body = crow::json::load(req.body);
            if (!body) {
                return dto::errorResponse(400, "malformed_request", "Malformed Request",
                                           "Request body is not valid JSON.");
            }

            kinetiq::app::LoginUser loginUser(users, hasher, tokens);
            try {
                const auto result = loginUser.execute(dto::parseLoginRequest(body));
                auto json = dto::authResultToJson(result.user, result.accessToken);
                return crow::response(200, json);
            } catch (const kinetiq::app::InvalidCredentialsError&) {
                // Same message for wrong password AND unknown email -- see
                // InvalidCredentialsError's comment in app/LoginUser.h.
                return dto::errorResponse(401, "invalid_credentials", "Unauthorized",
                                           "Invalid email or password.");
            } catch (const std::exception&) {
                return dto::errorResponse(422, "validation_error", "Validation Failed",
                                           "One or more required fields are missing or invalid.");
            }
        });

    CROW_ROUTE(app, "/api/v1/auth/me")
        ([&users, &tokens](const crow::request& req) {
            const auto currentUser = authenticate(req, tokens);
            if (!currentUser.has_value()) {
                return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                           "Missing, invalid, or expired token.");
            }

            const auto found = users.findById(currentUser->userId);
            if (!found.has_value()) {
                // Token was valid but the user it names no longer exists
                // (e.g. deleted between issuance and this request).
                return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                           "Missing, invalid, or expired token.");
            }

            return crow::response(200, dto::userToJson(*found));
        });

    CROW_ROUTE(app, "/api/v1/auth/password")
        .methods(crow::HTTPMethod::Patch)(
            [&users, &hasher, &tokens](const crow::request& req) {
                const auto currentUser = authenticate(req, tokens);
                if (!currentUser.has_value()) {
                    return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                               "Missing, invalid, or expired token.");
                }

                const auto body = crow::json::load(req.body);
                if (!body) {
                    return dto::errorResponse(400, "malformed_request", "Malformed Request",
                                               "Request body is not valid JSON.");
                }

                kinetiq::app::ChangePassword changePassword(users, hasher);
                try {
                    changePassword.execute(
                        dto::parseChangePasswordRequest(body, currentUser->userId));
                    return crow::response(200);
                } catch (const kinetiq::domain::WeakPasswordError&) {
                    return dto::errorResponse(422, "validation_error", "Validation Failed",
                                               "New password does not meet the strength policy.");
                } catch (const kinetiq::app::InvalidCredentialsError&) {
                    return dto::errorResponse(401, "invalid_credentials", "Unauthorized",
                                               "Current password is incorrect.");
                } catch (const std::exception&) {
                    return dto::errorResponse(422, "validation_error", "Validation Failed",
                                               "One or more required fields are missing or invalid.");
                }
            });
}

}  // namespace kinetiq::http
