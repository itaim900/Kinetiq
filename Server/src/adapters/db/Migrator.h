#pragma once

#include <pqxx/pqxx>

#include <filesystem>
#include <string>

namespace fitapp::db {

// Applies SQL migrations from a directory on startup.
//
// Files are named NNNN_description.sql (e.g. 0002_core_schema.sql) and applied
// in ascending filename order. Each unapplied migration runs inside its own
// transaction; on success its version (the filename without ".sql") is recorded
// in the schema_migrations table so it is never run twice. A failing migration
// aborts (its transaction rolls back) and the exception propagates, so the app
// refuses to start against a half-migrated database.
//
// This is a DB adapter: it depends on libpqxx + the filesystem only, never on
// Crow or the HTTP layer.
class Migrator {
public:
    struct Result {
        int applied = 0;
        int skipped = 0;
    };

    Migrator(pqxx::connection& conn, std::filesystem::path directory);

    // Ensures the bookkeeping table exists, then applies every pending migration
    // in order. Returns how many were applied vs. already-applied (skipped).
    Result run();

private:
    pqxx::connection& conn_;
    std::filesystem::path directory_;
};

}  // namespace fitapp::db
