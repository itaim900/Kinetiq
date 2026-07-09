# WORKLOG — shared running log

Append-only session log shared across **both** interfaces (Claude Code in VS Code / Cursor, and
Cowork). Newest entry at the top. This is how work done in one place carries over to the other:
whatever I do in a session, I record here before finishing.

**How to use:** at the start of a session, read this + `docs/CONTEXT.md`. At the end, add an entry
below with today's date. Keep entries short (3–8 lines): what changed, why, decisions, what's next.
For durable/architectural facts, also update `docs/CONTEXT.md` (and add an ADR when warranted).

---

## 2026-07-09 — VS Code
- Onboarded (fresh interface): re-read `docs/CONTEXT.md` + this log, confirmed Phase 1 + Phase 2
  Slice 0 status. Ran the full dev stack (Postgres, server, client) to verify the machine still
  works end-to-end — all green, 13/13 `ctest` passing.
- Did a guided code walkthrough of the `/api/v1/health` request path (Crow route in `main.cpp` →
  DB check → JSON response → Vite proxy → React `App.tsx`) for the owner's learning goals; noted
  the hexagonal folders (`http/ app/ domain/ ports/`) are still empty scaffolding until Slice 1.
- Found + fixed a real bug: background dev-server processes from past sessions (7/5, 7/6) were
  never terminated, left running as orphans, and squatted ports 5173/5174 — causing Vite to drift
  to 5175 in this session and confusing "is the port stable" diagnosis.
- Added `scripts/run-dev.ps1`: wraps server/client launch in a Windows Job Object
  (`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`) so the whole child process tree dies automatically if the
  wrapper process ends for any reason (Ctrl-C, window close, forced kill) — verified through 4
  levels of nesting for the client (npm → cmd → vite → node). Documented in
  `docs/learning/how-to-run.md` (git-ignored, not in this commit).
- Next: use `./scripts/run-dev.ps1 -Target server|client` going forward instead of raw
  exe/`npm run dev`. Continue Phase 2, Slice 1 (Auth) — see `docs/phase-2-plan.md`.

## 2026-07-08 — Cowork
- Set up cross-interface continuity: added root `CLAUDE.md` (auto-loaded by both VS Code and Cowork)
  and this `WORKLOG.md`. Both now point at `docs/CONTEXT.md` as the source of truth.
- Why: owner wants to work in VS Code while staying aligned here. Chat histories don't sync between
  interfaces, but shared files in the repo do — so context lives in files, not the transcript.
- Next: use this log going forward. Continue Phase 2, Slice 1 (Auth) — see `docs/phase-2-plan.md`.

<!-- Template for new entries (copy above this line):

## YYYY-MM-DD — <VS Code | Cowork>
- What changed:
- Why / decisions:
- Next:

-->
