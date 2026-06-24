#include "crow_all.h"

#include <pqxx/pqxx>

#include <exception>
#include <string>

#include "config/Env.h"

namespace {

std::string buildConnString() {
    return "host=" + fitapp::envOr("DB_HOST", "127.0.0.1") +
           " port=" + fitapp::envOr("DB_PORT", "5432") +
           " dbname=" + fitapp::envOr("DB_NAME", "fitapp") +
           " user=" + fitapp::envOr("DB_USER", "fitapp") +
           " password=" + fitapp::envOr("DB_PASSWORD", "fitapp");
}

}  // namespace

int main() {
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

    CROW_LOG_INFO << "Fit App backend listening on http://127.0.0.1:8080";
    app.bindaddr("127.0.0.1").port(8080).multithreaded().run();
    return 0;
}
