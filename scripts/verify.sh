#!/usr/bin/env bash
# Programmatic verification that stremio-accru is working — Rust + Frontend + Build
# Mirrors CI (.github/workflows/ci.yml) plus added programmatic tests per Tauri docs:
#   https://v2.tauri.app/develop/tests/ (mock runtime, mocks module)
#   https://v2.tauri.app/develop/tests/mocking/ (mockIPC, mockWindows)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

pass=0; fail=0
step() { echo ""; echo "━━━ $1 ━━━"; }

run() {
  local name="$1"; shift
  if "$@"; then
    echo "✅ $name"
    pass=$((pass+1))
  else
    echo "❌ $name"
    fail=$((fail+1))
  fi
}

step "1/6 cargo check (src-tauri)"
run "cargo check" cargo check --manifest-path src-tauri/Cargo.toml

step "2/6 cargo test (42 Rust unit tests — player/skip/tracks/hdr/svp/thumbs/state/backend + core/runtime + config/portable)"
run "cargo test" cargo test --manifest-path src-tauri/Cargo.toml

step "3/6 svelte-kit sync (generate .svelte-kit/tsconfig.json)"
run "svelte-kit sync" npx svelte-kit sync

step "4/6 frontend build (vite)"
run "vite build" npm run build

step "5/6 svelte-check (type)"
run "svelte-check" npm run check

step "6/6 vitest (27 frontend tests — hero config/cache/catalog + tauri mocks mockIPC/mockWindows/events)"
run "vitest" npx vitest run

echo ""
echo "═══════════════════════════════════════"
echo "Passed: $pass / $((pass+fail)) — Failed: $fail"
if [ "$fail" -eq 0 ]; then
  echo "✅ stremio-accru is working — all programmatic tests green"
  exit 0
else
  echo "❌ verification failed"
  exit 1
fi
