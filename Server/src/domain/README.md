# domain/

**Pure business rules and entities. No I/O, no framework, no database.**

Contains plain C++ types (e.g. `User`, `Injury`) and validators (e.g. "password must be ≥ 8 chars",
"weight_kg within range"). Everything here is deterministic and trivially unit-testable — it never
imports Crow, libpqxx, or any adapter.

Depends on: nothing outside the standard library.
