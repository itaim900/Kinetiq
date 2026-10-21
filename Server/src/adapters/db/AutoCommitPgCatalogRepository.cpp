#include "adapters/db/AutoCommitPgCatalogRepository.h"

#include "adapters/db/PgCatalogRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

namespace {
// Runs `fn` against a PgCatalogRepository inside its own connection +
// transaction. Read-only, but commit anyway so the transaction ends cleanly.
template <typename Fn>
auto withCatalog(const std::string& connString, Fn fn) {
    pqxx::connection conn(connString);
    pqxx::work tx(conn);
    PgCatalogRepository repo(tx);
    auto result = fn(repo);
    tx.commit();
    return result;
}
}  // namespace

std::vector<kinetiq::domain::BodyRegion> AutoCommitPgCatalogRepository::listBodyRegions() {
    return withCatalog(connString_, [](PgCatalogRepository& r) { return r.listBodyRegions(); });
}

bool AutoCommitPgCatalogRepository::bodyRegionExists(const std::string& slug) {
    return withCatalog(connString_,
                       [&slug](PgCatalogRepository& r) { return r.bodyRegionExists(slug); });
}

std::vector<kinetiq::domain::Muscle> AutoCommitPgCatalogRepository::listMuscles() {
    return withCatalog(connString_, [](PgCatalogRepository& r) { return r.listMuscles(); });
}

std::optional<kinetiq::domain::Muscle> AutoCommitPgCatalogRepository::findMuscle(long long id) {
    return withCatalog(connString_, [id](PgCatalogRepository& r) { return r.findMuscle(id); });
}

std::vector<kinetiq::domain::InjuryCondition> AutoCommitPgCatalogRepository::listConditions() {
    return withCatalog(connString_, [](PgCatalogRepository& r) { return r.listConditions(); });
}

std::optional<kinetiq::domain::InjuryCondition> AutoCommitPgCatalogRepository::findCondition(
    long long id) {
    return withCatalog(connString_, [id](PgCatalogRepository& r) { return r.findCondition(id); });
}

}  // namespace kinetiq::db
