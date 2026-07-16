#include "crow_all.h"

#include <pqxx/pqxx>

#include <chrono>
#include <exception>
#include <string>
#include <thread>

#include "adapters/crypto/Argon2PasswordHasher.h"
#include "adapters/crypto/JwtTokenService.h"
#include "adapters/db/AutoCommitPgUserRepository.h"
#include "adapters/db/Migrator.h"
#include "config/Env.h"
#include "http/controllers/AuthController.h"

namespace {

std::string buildConnString() {
    return "host=" + kinetiq::envOr("DB_HOST", "127.0.0.1") +
           " port=" + kinetiq::envOr("DB_PORT", "5432") +
           " dbname=" + kinetiq::envOr("DB_NAME", "kinetiq") +
           " user=" + kinetiq::envOr("DB_USER", "kinetiq") +
           " password=" + kinetiq::envOr("DB_PASSWORD", "kinetiq");
}

// Connect with a few retries so we tolerate the DB still warming up (e.g. the
// Postgres container booting alongside us). Throws if it never comes up.
pqxx::connection connectWithRetry() {
    constexpr int maxAttempts = 5;
    for (int attempt = 1;; ++attempt) {
        try {
            return pqxx::connection(buildConnString());
        } catch (const std::exception& e) {
            if (attempt >= maxAttempts) throw;
            CROW_LOG_WARNING << "DB not ready (attempt " << attempt << "/" << maxAttempts
                             << "): " << e.what();
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
}

// Apply pending migrations before serving. If this fails, the server must not
// start: running against a half-migrated schema is worse than being down.
bool runMigrations() {
    try {
        pqxx::connection conn = connectWithRetry();
        kinetiq::db::Migrator migrator(conn, kinetiq::envOr("MIGRATIONS_DIR", "migrations"));
        migrator.run();
        return true;
    } catch (const std::exception& e) {
        CROW_LOG_ERROR << "migrations failed, refusing to start: " << e.what();
        return false;
    }
}

}  // namespace

int main() {
    if (!runMigrations()) {
        return 1;
    }

    crow::SimpleApp app;

    CROW_ROUTE(app, "/api/v1/health")
    ([]() {
        crow::json::wvalue body;
        body["status"] = "ok";
        try {
            pqxx::connection conn(buildConnString());
            pqxx::work tx(conn);
            tx.exec("SELECT 1");
            tx.commit();
            body["db"] = "connected";
        } catch (const std::exception&) {
            body["db"] = "unavailable";
        }
        crow::response res(body);
        res.add_header("Access-Control-Allow-Origin", "*");  // dev convenience
        return res;
    });

    // --- Slice 1: auth ---
    // JWT_SECRET has an insecure dev-only fallback, same convention as
    // DB_PASSWORD below -- but unlike that one, warn loudly if it's in use,
    // since a guessable JWT secret lets anyone forge a valid login token for
    // any user (a leaked/default DB password is comparatively contained).
    const std::string jwtSecretFallback = "dev-only-insecure-secret-change-in-production";
    const std::string jwtSecret = kinetiq::envOr("JWT_SECRET", jwtSecretFallback);
    if (jwtSecret == jwtSecretFallback) {
        CROW_LOG_WARNING << "JWT_SECRET not set -- using an insecure default. "
                             "Set a real secret before deploying.";
    }

    kinetiq::db::AutoCommitPgUserRepository users(buildConnString());
    kinetiq::crypto::Argon2PasswordHasher hasher;
    kinetiq::crypto::JwtTokenService tokens(jwtSecret, std::chrono::hours(24));
    kinetiq::http::registerAuthRoutes(app, users, hasher, tokens);

    // Bind to 127.0.0.1 by default (safe for local dev). In containers set
    // BIND_ADDR=0.0.0.0 so other containers (e.g. the frontend) can reach it.
    const std::string bindAddr = kinetiq::envOr("BIND_ADDR", "127.0.0.1");
    CROW_LOG_INFO << "KinetiQ backend listening on http://" << bindAddr << ":8080";
    app.bindaddr(bindAddr).port(8080).multithreaded().run();
    return 0;
}
