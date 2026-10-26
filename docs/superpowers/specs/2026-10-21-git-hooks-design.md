# Git hooks: local commit/push checks (design)

**Date:** 2026-10-21 · **Status:** Approved (brainstorming) · **Branch:** `slice-2-profile`

## Goal
Catch broken builds and failing tests on the developer's machine before they reach GitHub, without
making the small-commit TDD rhythm slow. GitHub Actions CI (`.github/workflows/ci.yml`) remains the
final authority; the hooks are an earlier, local safety net.

## Measured baseline (2026-10-21, 8 GB machine, nothing to recompile)
| Check | Time |
|---|---|
| Server incremental build (no changes) | ~3 s |
| Server tests via `ctest` (one process per test) | ~40 s |
| Server tests via `kinetiq_tests` binary directly | ~13 s |
| Client `tsc --noEmit` | ~9 s |
| Client `vitest run` | ~13 s |

A C++ source change adds compile time (can be minutes for widely-included headers).

## Decisions
| # | Decision | Why |
|---|---|---|
| H1 | **pre-commit checks only the changed half; pre-push runs everything** | Keeps frequent TDD commits fast; nothing unverified leaves the machine. |
| H2 | **Skipped DB tests: warn on commit, block on push** | Commits work with Docker closed (saves RAM); pushes always reflect all tests. |
| H3 | **Plain shell scripts in tracked `.githooks/`**, enabled with `git config core.hooksPath .githooks` | `.git/hooks/` isn't versioned; no Husky (no root `package.json`) or Python `pre-commit` dependency; owner learns the real mechanism. |
| H4 | **Detect "tests skipped", not "Postgres reachable"** | GoogleTest prints `[  SKIPPED ] N tests`. This catches both Postgres being down *and* a broken migration (which `DbTest` turns into a whole-suite skip). A port probe misses the second. |
| H5 | **Run the `kinetiq_tests` binary directly, not `ctest`** | Same 122 tests, ~3x faster. CI keeps using `ctest`. |
| H6 | **Hooks check the working tree, not the exact staged snapshot** | Stash-based snapshot testing risks losing work if interrupted; small commits + full pre-push check cover the gap. |

## Files
```
.githooks/
├── pre-commit   # decides which halves changed, calls helpers
├── pre-push     # calls every helper, strict about skips
└── lib.sh       # shared helpers (sourced, not executed directly)
```

### `lib.sh` helpers
- **Environment setup:** prepend `/c/msys64/ucrt64/bin` to `PATH` when it exists (no-op on Linux);
  `cd` to the repo root (`git rev-parse --show-toplevel`).
- **`server_check <mode>`** (`mode` = `warn` | `strict`):
  1. If `Server/build/` lacks a configured build, run `cmake --preset default` first.
  2. `cmake --build --preset default`.
  3. Run `Server/build/kinetiq_tests.exe` (or `kinetiq_tests` without `.exe` on Linux).
  4. Parse the `[  SKIPPED ] N` line. If N > 0: `warn` prints a yellow warning and returns
     success; `strict` prints the warning as an error and returns failure. The message says
     to run `docker start kinetiq-postgres` and that a broken migration causes the same symptom.
- **`client_check`:** `npx tsc --noEmit`, then `npx vitest run` (in `Client/`).
- **`client_build`:** `npm run build` (in `Client/`; same step as CI).
- **Step runner:** each step prints one line with its name, `ok`/`FAILED` and elapsed seconds. Step
  output goes to a temp log; on failure the last ~40 lines of the log are printed.
- **Precondition errors** stop with fix instructions:
  - `cmake`/`g++` missing → install MSYS2 UCRT64 / check `C:\msys64\ucrt64\bin`.
  - `Client/node_modules` missing → run `npm install` in `Client/`.

### `pre-commit`
1. `git diff --cached --name-only` → staged paths.
2. Any path under `Server/` → `server_check warn`.
3. Any path under `Client/` → `client_check`.
4. Neither → print "no app code changed, skipping checks" and exit 0.
5. Any failure → exit non-zero (commit blocked); print that `git commit --no-verify` bypasses the
   checks in an emergency (e.g. a deliberate TDD "red" commit).

### `pre-push`
Runs `server_check strict`, `client_check` and `client_build`, regardless of what changed. Any failure
blocks the push.

## Setup & docs
- One-time per clone: `git config core.hooksPath .githooks`.
- Document in `CLAUDE.md` (Commands section) and `README.md` (Running locally).
- Executable bit: commit the hook files with mode 755 (`git update-index --chmod=+x`) so they also
  run on Linux/macOS clones. Git for Windows runs them regardless.
- **Line endings:** this machine has `core.autocrlf=true`, which would check scripts out with CRLF
  endings; `sh` then fails on the stray `\r` (`bad interpreter: /bin/sh^M`). Add a root
  `.gitattributes` with `.githooks/* text eol=lf` and `*.sh text eol=lf` so hook scripts are
  always LF, on every OS.

## Verification (manual checklist, run once after implementation)
1. Commit of docs-only change → allowed, no checks run.
2. Commit touching only `Client/` → only client checks run.
3. Commit touching only `Server/` → only server checks run.
4. A deliberately failing test → commit blocked (then revert the change).
5. Postgres stopped + `Server/` commit → skip warning shown, commit allowed.
6. Postgres stopped + push → push blocked with the skip message.
7. Everything healthy + push → all checks pass, push proceeds.

## Out of scope (YAGNI)
Auto-formatting (clang-format/Prettier), linting, commit-message conventions and automated tests of
the hook scripts themselves. Easy to add to the same scripts later.
