# Architecture Decision Records (ADRs)

An ADR captures a single significant decision: the **context**, the **options considered**, the
**decision**, and its **consequences**. They exist so that future-you (and anyone reading the
repo) understands *why* the project is built the way it is — not just *what* it does.

Format: lightweight [MADR](https://adr.github.io/madr/)-style. Each ADR is immutable once
accepted; if a decision changes, add a new ADR that supersedes the old one.

## Index

| # | Decision | Status |
|---|---|---|
| [0001](./0001-backend-language-cpp.md) | Backend language: C++ | Accepted |
| [0002](./0002-hexagonal-architecture.md) | Architecture style: Hexagonal (Ports & Adapters) | Accepted |
| [0003](./0003-cpp-web-framework-drogon.md) | C++ web framework: Drogon | Superseded by 0011 |
| [0004](./0004-database-postgresql.md) | Database: PostgreSQL | Accepted |
| [0005](./0005-build-and-dependencies.md) | Build & dependencies: CMake + vcpkg | Superseded by 0012 |
| [0006](./0006-authentication-jwt.md) | Authentication: short-lived JWT (v1) | Accepted |
| [0007](./0007-backend-testing-googletest.md) | Backend testing: GoogleTest + TDD | Accepted |
| [0008](./0008-llm-provider-gemini.md) | LLM provider: Gemini behind `ILLMClient` | Accepted |
| [0009](./0009-hybrid-ai-boundary.md) | Hybrid deterministic + generative AI boundary | Accepted |
| [0010](./0010-frontend-stack.md) | Frontend stack: React + TypeScript + Vite | Accepted |
| [0011](./0011-web-framework-crow.md) | C++ web framework: Crow (supersedes 0003) | Accepted |
| [0012](./0012-toolchain-msys2.md) | Toolchain: MSYS2 UCRT64 + prebuilt deps (supersedes 0005) | Accepted |
| [0013](./0013-authenticator-function-not-middleware.md) | Auth is a plain function, not Crow middleware | Accepted |
| [0014](./0014-load-modify-save-patch.md) | PATCH = load-modify-validate-save | Accepted |
| [0015](./0015-autocommit-repository-pattern.md) | Two-layer repositories (Pg + AutoCommit) | Accepted |
| [0016](./0016-injury-conditions-catalog.md) | Injuries via conditions catalog (known = locked) | Accepted |
| [0017](./0017-merge-patch-clearing-and-profile-history.md) | JSON Merge Patch clearing + profile history | Accepted |
