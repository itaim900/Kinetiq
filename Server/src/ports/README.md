# ports/

**Interfaces (contracts) that the core depends on — the "ports" of Ports & Adapters.**

Abstract interfaces such as `IUserRepository` (save/find users) and `ITokenService` (issue/verify
JWTs). They declare *what* can be done, never *how*. Concrete implementations live in `adapters/`.

This indirection is what makes the code test-driven: tests plug in in-memory fakes; production plugs
in the real Postgres/crypto adapters.
