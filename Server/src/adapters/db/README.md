# adapters/db/

**Concrete implementations of repository ports, backed by PostgreSQL (libpqxx).**

E.g. `PgUserRepository implements IUserRepository`. This is the only place that writes SQL. Swapped
for in-memory fakes in unit tests; exercised directly in integration tests (against a real test DB).
