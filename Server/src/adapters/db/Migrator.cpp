#include "adapters/db/Migrator.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace kinetiq::db {

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open migration file: " + path.string());
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

Migrator::Migrator(pqxx::connection& conn, std::filesystem::path directory)
    : conn_(conn), directory_(std::move(directory)) {}

Migrator::Result Migrator::run() {
    // 1. Bookkeeping table (owned by the migrator, not by any migration file).
    {
        pqxx::work tx(conn_);
        tx.exec(
            "CREATE TABLE IF NOT EXISTS schema_migrations ("
            "  version    TEXT PRIMARY KEY,"
            "  applied_at TIMESTAMPTZ NOT NULL DEFAULT now())");
        tx.commit();
    }

    // 2. Which versions are already applied?
    std::set<std::string> applied;
    {
        pqxx::work tx(conn_);
        for (const auto& row : tx.exec("SELECT version FROM schema_migrations")) {
            applied.insert(row[0].as<std::string>());
        }
        tx.commit();
    }

    // 3. Collect *.sql files and sort by filename (NNNN_ prefix => run order).
    if (!std::filesystem::is_directory(directory_)) {
        throw std::runtime_error("migrations directory not found: " + directory_.string());
    }
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".sql") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    // 4. Apply each pending migration in its own transaction.
    Result result;
    for (const auto& file : files) {
        const std::string version = file.stem().string();  // filename without ".sql"
        if (applied.contains(version)) {
            ++result.skipped;
            continue;
        }

        std::cout << "[migrate] applying " << version << std::endl;
        const std::string sql = readFile(file);

        pqxx::work tx(conn_);
        tx.exec(sql);  // libpq runs multiple ';'-separated statements in one call
        // version is a trusted filename, but quote() keeps this API-stable across
        // libpqxx versions (exec_params vs exec(zview, params) differ by version).
        tx.exec("INSERT INTO schema_migrations (version) VALUES (" + tx.quote(version) + ")");
        tx.commit();
        ++result.applied;
    }

    std::cout << "[migrate] done: " << result.applied << " applied, " << result.skipped
              << " already up to date" << std::endl;
    return result;
}

}  // namespace kinetiq::db
