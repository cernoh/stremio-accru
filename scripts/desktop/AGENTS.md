# Purpose

- Owns the standalone desktop app (issue #43): `deno.json`, `main.ts`,
  `stremio-accru.desktop`. Runs on MangoWM via `nix run .#app`.

# Ownership

- `scripts/AGENTS.md` owns the launcher contract; this doc owns the
  `deno desktop` layer on top of it.

# Local Contracts

- `deno desktop` always compiles; `nix run .#app` builds to
  `$XDG_CACHE_HOME/stremio-accru/bundle` (rebuilt when `${./scripts}`
  changes) and patchelf-patches the bundle for NixOS stub-ld.
- Backend binaries live in `~/.cache/deno/laufey/` (NOT in-repo) and need
  manual patchelf after (re)download: webview (interpreter+rpath), CEF
  (+ `$ORIGIN` rpath, `libEGL.so.1`/`libGLESv2.so.1` symlinks). Redo after
  cache clear or backend version bump.
- `main.ts` is zero-dependency (Deno + `node:` builtins only).
- Server/seed stay owned by `../stremio-linux.sh`; this layer never
  reimplements them.
- MangoWM match goes by window appid; rule example lives in `--help`.
  Confirm the real appid in the compositor client list before relying on it.

# Work Guidance

- Keep permissions broad but documented (local app, baked at compile).
- Window geometry persists to `$DATA_DIR/window.json` per upstream pattern.

# Verification

- `deno check main.ts`, `deno lint main.ts`, `deno fmt --check main.ts`
  (run from `scripts/desktop/` inside `nix develop`).
- `nix run .#app -- --check`; visual: `nix run .#app` on MangoWM.

# Child DOX Index

- No child docs.
