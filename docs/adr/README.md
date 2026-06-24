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
| [0003](./0003-cpp-web-framework-drogon.md) | C++ web framework: Drogon | Accepted |
| [0004](./0004-database-postgresql.md) | Database: PostgreSQL | Accepted |
| [0005](./0005-build-and-dependencies.md) | Build & dependencies: CMake + vcpkg | Accepted |
| [0006](./0006-authentication-jwt.md) | Authentication: short-lived JWT (v1) | Accepted |
| [0007](./0007-backend-testing-googletest.md) | Backend testing: GoogleTest + TDD | Accepted |
| [0008](./0008-llm-provider-gemini.md) | LLM provider: Gemini behind `ILLMClient` | Accepted |
| [0009](./0009-hybrid-ai-boundary.md) | Hybrid deterministic + generative AI boundary | Accepted |
| [0010](./0010-frontend-stack.md) | Frontend stack: React + TypeScript + Vite | Accepted |
