# CLAUDE.md — start-of-session instructions

This file is auto-loaded by Claude in **every** interface (Claude Code in VS Code / Cursor,
and Cowork). It is the shared entry point that keeps work aligned no matter where I'm typing.

## First thing, every session
1. **Read `docs/CONTEXT.md`** — the dense state-of-the-project brief (stack, architecture,
   current phase, decisions, how to run). It is the source of truth for *where things stand*.
2. **Read `WORKLOG.md`** (repo root) — the running, append-only log of what happened in recent
   sessions across both VS Code and Cowork. This is how the two interfaces "stay connected":
   whatever I did elsewhere is written here.

## End of every working session (both interfaces)
Append a dated entry to `WORKLOG.md` (newest at top) covering: what changed, why, decisions made,
and what's next. Keep it short — 3–8 lines. If a decision is durable/architectural, also update
`docs/CONTEXT.md` (and add an ADR in `docs/adr/` when warranted).

## Owner & working style
Owner is a **beginner** at C++ and React building this as a **learning + portfolio** project.
Prefer clear explanations, small steps, and planning over fast code. Explain *why*, not just *what*.

## Project one-liner
AI-powered personalized workout-plan web app. **Server:** C++20 / Crow / libpqxx / Postgres.
**Client:** React 18 + TS + Vite. Hexagonal architecture. Full detail in `docs/CONTEXT.md`.

## Conventions to respect (see CONTEXT.md for the reasoning)
- Source lives in `Server/` and `Client/` only — never recreate `backend/` or `frontend/` folders.
- 8 GB RAM machine: don't run a heavy C++ build and Docker at the same time.
- Container images are built in CI, not locally. Migrations run via the C++ startup runner.
- Test-first (TDD) for Phase 2 domain work.
