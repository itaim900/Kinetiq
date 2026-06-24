# Fit App

An AI-powered web application that helps people create personalized workout plans.

## Overview

Fit App uses an AI agent to generate workout plans tailored to a user's goals,
experience level, available equipment, and schedule.

- **Backend:** C++ (Crow + libpqxx), built with CMake + Ninja on the MSYS2 UCRT64 toolchain
- **Frontend:** React + TypeScript (Vite)
- **Database:** PostgreSQL (Docker Compose)
- **AI:** LLM-based agent for personalized plan generation (Phase 3)

> This is a learning + portfolio project. Architecture and scope are evolving.

## Project structure

```
.
├── backend/            # C++ API server (Crow)
│   ├── src/            # application code
│   ├── tests/          # GoogleTest unit tests
│   ├── migrations/     # ordered SQL migrations
│   ├── third_party/    # vendored headers (Crow)
│   ├── CMakeLists.txt
│   └── CMakePresets.json
├── frontend/           # React + TypeScript web app (Vite)
├── docs/               # Design docs, architecture notes, decisions
└── docker-compose.yml  # local PostgreSQL
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

# 3. Build + test the backend (from an MSYS2 UCRT64 shell, in backend/)
cmake --preset default && cmake --build --preset default && ctest --test-dir build

# 4. Run the backend (DB_PORT must match docker-compose host port)
./build/fitapp_server.exe

# 5. Frontend (in frontend/)
npm install && npm run dev   # http://localhost:5173, proxies /api -> backend
```

Health check: `GET http://localhost:8080/api/v1/health` →
`{"status":"ok","db":"connected"}`.

## Design docs

The project is being designed before implementation. See [`docs/`](./docs):

- [`01-data-model.md`](./docs/01-data-model.md) — database schema, ER diagram, design principles
- [`02-api-contract.md`](./docs/02-api-contract.md) — REST API contract (contract-first, with test cases)
- [`03-llm-port.md`](./docs/03-llm-port.md) — AI subsystem: LLM port, validation, weight engine
- [`04-architecture.md`](./docs/04-architecture.md) — system architecture (Ports & Adapters), folder layout
- [`05-roadmap.md`](./docs/05-roadmap.md) — phased roadmap, milestones, definitions of done
- [`adr/`](./docs/adr/README.md) — Architecture Decision Records

## Status

**Phase 1 (walking skeleton) in progress.** The toolchain is proven end-to-end: React (Vite) →
dev proxy → C++ Crow backend → libpqxx → PostgreSQL, with `GET /api/v1/health` returning live
DB status, GoogleTest and Vitest harnesses wired, and the first migration applied. See
[`docs/05-roadmap.md`](./docs/05-roadmap.md) and the [issues](https://github.com/itaim900/Fit-App/issues).
