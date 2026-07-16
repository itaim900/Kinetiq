#pragma once

#include "app/ChangePassword.h"
#include "app/LoginUser.h"
#include "app/RegisterUser.h"
#include "domain/User.h"

#include "crow_all.h"

#include <string>

namespace fitapp::http::dto {

// Parses a POST /auth/register request body into RegisterUserRequest.
inline fitapp::app::RegisterUserRequest parseRegisterRequest(const crow::json::rvalue& body) {
    fitapp::app::RegisterUserRequest request;
    request.email = std::string(body["email"].s());
    request.password = std::string(body["password"].s());
    request.displayName = std::string(body["display_name"].s());
    return request;
}

// Parses a POST /auth/login request body into LoginUserRequest.
inline fitapp::app::LoginUserRequest parseLoginRequest(const crow::json::rvalue& body) {
    fitapp::app::LoginUserRequest request;
    request.email = std::string(body["email"].s());
    request.password = std::string(body["password"].s());
    return request;
}

// Parses a PATCH /auth/password request body, pairing it with the
// authenticated user's id (from the token, never from the request body).
inline fitapp::app::ChangePasswordRequest parseChangePasswordRequest(
    const crow::json::rvalue& body, long long userId) {
    fitapp::app::ChangePasswordRequest request;
    request.userId = userId;
    request.currentPassword = std::string(body["current_password"].s());
    request.newPassword = std::string(body["new_password"].s());
    return request;
}

// The `user` sub-object shared by register/login/me responses. Deliberately
// excludes passwordHash -- never serialize it onto the wire.
inline crow::json::wvalue userToJson(const fitapp::domain::User& user) {
    crow::json::wvalue json;
    json["id"] = user.id;
    json["email"] = user.email;
    json["display_name"] = user.displayName;
    return json;
}

// { user: {...}, access_token: "..." } -- register/login share this shape.
inline crow::json::wvalue authResultToJson(const fitapp::domain::User& user,
                                            const std::string& accessToken) {
    crow::json::wvalue json;
    json["user"] = userToJson(user);
    json["access_token"] = accessToken;
    return json;
}

// RFC 9457 Problem Details shape (see docs/02-api-contract.md "Conventions"),
// simplified: no per-field `errors` map yet, since our exception types don't
// carry field names. Shared across all auth endpoints' error responses.
inline crow::response errorResponse(int status, const std::string& type, const std::string& title,
                                     const std::string& detail) {
    crow::json::wvalue json;
    json["type"] = type;
    json["title"] = title;
    json["status"] = status;
    json["detail"] = detail;
    return crow::response(status, json);
}

}  // namespace fitapp::http::dto
