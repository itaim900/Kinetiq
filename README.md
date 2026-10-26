# Kinetiq

An AI-powered web application that helps people create personalized workout plans.

## Overview

Kinetiq uses an AI agent to generate workout plans tailored to a user's goals,
experience level, available equipment, and schedule.

- **Backend:** C++ (Crow + libpqxx), built with CMake + Ninja on the MSYS2 UCRT64 toolchain
- **Frontend:** React + TypeScript (Vite)
- **Database:** PostgreSQL (Docker Compose)
- **AI:** LLM-based agent for personalized plan generation (Phase 3)

> This is a learning + portfolio project. Architecture and scope are evolving.

## Project structure

```
.
├── Server/             # C++ API server (Crow)
│   ├── src/            # application code
│   ├── tests/          # GoogleTest unit tests
│   ├── migrations/     # ordered SQL migrations
│   ├── third_party/    # vendored headers (Crow)
│   ├── CMakeLists.txt
│   └── CMakePresets.json
├── Client/             # React + TypeScript web app (Vite) + Dockerfile + nginx.conf
├── docs/               # Design docs, architecture notes, decisions
├── docker-compose.yml  # local PostgreSQL (dev)
└── docker-compose.prod.yml  # full app (Server + Client + Postgres) from GHCR images
```

## Running locally (Phase 1 walking skeleton)

Prerequisites: [MSYS2](https://www.msys2.org/) (UCRT64), Docker Desktop, Node.js.

```sh
# 1. Install backend deps (prebuilt, via MSYS2 pacman)
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-asio \
  mingw-w64-ucrt-x86_64-nlohmann-json mingw-w64-ucrt-x86_64-libpqxx \
  mingw-w64-ucrt-x86_64-gtest

# 2. Start PostgreSQL (set POSTGRES_HOST_PORT if 5432 is taken)
docker compose up -d

# 3. Build + test the server (from an MSYS2 UCRT64 shell, in Server/)
cmake --preset default && cmake --build --preset default && ctest --test-dir build

# 4. Run the server
./build/kinetiq_server.exe

# 5. Client (in Client/)
npm install && npm run dev   # http://localhost:5173, proxies /api -> server
```

Health check: `GET http://localhost:8080/api/v1/health` →
`{"status":"ok","db":"connected"}`.

**Git hooks (once per clone):** `git config core.hooksPath .githooks`. Commits check the part of the
app you changed; pushes run the full build + test suite (Postgres must be running).

## Running the whole app with Docker (any machine)

No toolchain required — only Docker. Prebuilt images are published to GHCR by CI.

```sh
docker compose -f docker-compose.prod.yml pull
docker compose -f docker-compose.prod.yml up -d
# open http://localhost:3000
```

If the images are private, either make the GHCR packages public or `docker login ghcr.io` with a
`read:packages` token first. See [`docs/learning/how-to-run.md`](./docs/learning/how-to-run.md).

## Design docs

The project is being designed before implementation. See [`docs/`](./docs):

- [`01-data-model.md`](./docs/01-data-model.md) — database schema, ER diagram, design principles
- [`02-api-contract.md`](./docs/02-api-contract.md) — REST API contract (contract-first, with test cases)
- [`03-llm-port.md`](./docs/03-llm-port.md) — AI subsystem: LLM port, validation, weight engine
- [`04-architecture.md`](./docs/04-architecture.md) — system architecture (Ports & Adapters), folder layout
- [`05-roadmap.md`](./docs/05-roadmap.md) — phased roadmap, milestones, definitions of done
- [`adr/`](./docs/adr/README.md) — Architecture Decision Records

## Status

**Phase 1 (walking skeleton) complete.** The toolchain is proven end-to-end: React (Vite) →
dev proxy → C++ Crow backend → libpqxx → PostgreSQL, with `GET /api/v1/health` returning live
DB status, GoogleTest and Vitest harnesses wired, and the first migration applied. **CI** builds and
tests both halves on every push, and the app is **containerized** (images published to GHCR, full
stack via `docker-compose.prod.yml`). Next: Phase 2 (core domain). See
[`docs/05-roadmap.md`](./docs/05-roadmap.md) and the [issues](https://github.com/itaim900/Kinetiq/issues).
