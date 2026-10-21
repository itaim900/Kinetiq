# shellcheck shell=bash
# Shared helpers for .githooks/pre-commit and .githooks/pre-push.
# This file is SOURCED (". lib.sh"), never executed directly. The caller must
# set HOOK_NAME first; it prefixes every line we print.

# --- Environment -----------------------------------------------------------

# Hooks can be started from any directory; always work from the repo root.
REPO_ROOT="$(git rev-parse --show-toplevel)" || exit 1
cd "$REPO_ROOT" || exit 1

# The C++ toolchain (g++, cmake, ninja) lives in MSYS2 UCRT64 on Windows.
# The folder doesn't exist on Linux/CI, so this is a no-op there.
if [ -d /c/msys64/ucrt64/bin ]; then
  PATH="/c/msys64/ucrt64/bin:$PATH"
fi

# Each step writes its full output here; we only show it when a step fails.
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

# Colors only when printing to a real terminal (not e.g. VS Code's git log).
if [ -t 1 ]; then
  RED=$'\033[31m'; YEL=$'\033[33m'; GRN=$'\033[32m'; RST=$'\033[0m'
else
  RED=''; YEL=''; GRN=''; RST=''
fi

# --- Output ----------------------------------------------------------------

say()  { printf '[%s] %s\n' "$HOOK_NAME" "$*"; }
warn() { printf '%s[%s] WARNING: %s%s\n' "$YEL" "$HOOK_NAME" "$*" "$RST"; }
fail() { printf '%s[%s] %s%s\n' "$RED" "$HOOK_NAME" "$*" "$RST" >&2; }

# run_step "<label>" <command> [args...]
# Runs the command with all output captured to $LOG and prints ONE status
# line with the elapsed time. On failure, prints the tail of the log so the
# real error is visible without scrolling through passing output.
run_step() {
  local label="$1"; shift
  local start=$SECONDS
  printf '[%s] %-36s ' "$HOOK_NAME" "$label"
  if "$@" >"$LOG" 2>&1; then
    printf '%sok%s (%ss)\n' "$GRN" "$RST" "$((SECONDS - start))"
  else
    printf '%sFAILED%s (%ss)\n' "$RED" "$RST" "$((SECONDS - start))"
    echo "----- last 40 lines of output -----"
    tail -n 40 "$LOG"
    echo "-----------------------------------"
    return 1
  fi
}

# --- Preconditions ---------------------------------------------------------

require_server_tools() {
  local tool
  for tool in cmake ninja g++; do
    if ! command -v "$tool" >/dev/null 2>&1; then
      fail "'$tool' not found. Install the MSYS2 UCRT64 toolchain (see README)"
      fail "and check that C:\\msys64\\ucrt64\\bin exists."
      return 1
    fi
  done
}

require_client_deps() {
  if [ ! -d Client/node_modules ]; then
    fail "Client/node_modules is missing. Run 'npm install' in Client/ first."
    return 1
  fi
}

# --- Server ----------------------------------------------------------------

# Small wrappers so run_step gets a single command. The ( ... ) subshell keeps
# the `cd` from leaking into the rest of the hook.
server_configure() { (cd Server && cmake --preset default); }
server_build()     { (cd Server && cmake --build --preset default); }
server_tests() {
  # .exe on Windows, no extension on Linux.
  if [ -f Server/build/kinetiq_tests.exe ]; then
    Server/build/kinetiq_tests.exe
  else
    Server/build/kinetiq_tests
  fi
}

# server_check warn|strict
# Builds the server and runs all its tests. DB integration tests SKIP (not
# fail) when Postgres is unreachable or a migration is broken, and the test
# binary still exits 0 -- so we read GoogleTest's summary line
# "[  SKIPPED ] N tests, listed below:" ourselves.
#   warn   -> print a warning, still succeed   (pre-commit)
#   strict -> treat skips as a failure         (pre-push)
server_check() {
  local mode="$1"
  require_server_tools || return 1

  # Fresh clone: no build folder configured yet.
  if [ ! -f Server/build/CMakeCache.txt ]; then
    run_step "Server: configure (first time)" server_configure || return 1
  fi
  run_step "Server: build" server_build || return 1
  run_step "Server: tests" server_tests || return 1

  local passed skipped
  passed="$(sed -n 's/^\[  PASSED  \] \([0-9][0-9]*\) test.*/\1/p' "$LOG" | head -n 1)"
  skipped="$(sed -n 's/^\[  SKIPPED \] \([0-9][0-9]*\) test.*/\1/p' "$LOG" | head -n 1)"
  say "  -> ${passed:-0} passed, ${skipped:-0} skipped"

  if [ "${skipped:-0}" -gt 0 ]; then
    local msg="${skipped} server test(s) were SKIPPED. Usually the DB tests: is Postgres running? (docker start kinetiq-postgres). A broken migration causes the same symptom."
    if [ "$mode" = strict ]; then
      fail "$msg"
      return 1
    fi
    warn "$msg"
  fi
}

# --- Client ----------------------------------------------------------------

client_typecheck() { (cd Client && npx tsc --noEmit); }
client_tests()     { (cd Client && npx vitest run); }
client_prod_build() { (cd Client && npm run build); }

client_check() {
  require_client_deps || return 1
  run_step "Client: type-check" client_typecheck || return 1
  run_step "Client: tests" client_tests || return 1
}

client_build() {
  require_client_deps || return 1
  run_step "Client: production build" client_prod_build
}
