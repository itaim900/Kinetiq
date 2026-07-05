# app/

**Application services (use cases) — orchestration of the domain and ports.**

Each service implements one use case (e.g. `RegisterUser`: validate input → hash password → persist
via `IUserRepository`). Services depend only on `domain/` and `ports/` interfaces, never on concrete
adapters — so they can be unit-tested against fakes.
