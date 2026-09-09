# Purpose

- Owns the legacy standalone desktop app (issue #43): `deno.json`,
  `main.ts`, `stremio-accru.desktop`. On Linux this layer is superseded by
  the native player (`player/`, #58) — kept here because the tag release
  workflow (#46) still compiles `main.ts` for Windows and macOS.

# Ownership

- `scripts/AGENTS.md` owns the launcher contract; this doc owns the
  `deno desktop` layer on top of it. `player/AGENTS.md` owns the Linux
  native player.

# Local Contracts

- `deno desktop` always compiles; on Linux the flake no longer builds this
  app (`.#app` is the native player, #58); the release workflow compiles
  it directly with `deno compile`.
- Backend is `webview` (`deno.json`): CEF renders but never commits a
  frame on MangoWM (toplevel acked, zero `wl_surface.attach`, viz wedged;
  reference Brave maps fine). See issue #44 for the forensics.
- Backend binaries live in `~/.cache/deno/laufey/` (NOT in-repo). The
  wrapper self-heals the webview backend on every launch (re-patches
  interpreter+rpath when it drifts after re-download/version bump).
  CEF needs manual patchelf after (re)download and is still broken on
  MangoWM, so it stays manual.
- MangoWM runtime env (set by the wrapper and the dev-shell hook):
  `WEBKIT_DISABLE_DMABUF_RENDERER=1` (Mango enforces explicit-sync
  acquire points; the dmabuf renderer is killed with Gdk Error 71),
  `GIO_EXTRA_MODULES` (glib-networking TLS, else `https://` shows
  `TLS support is not available`).
- `main.ts` is zero-dependency (Deno + `node:` builtins only).
- Server/seed stay owned by `../stremio-linux.sh`; this layer never
  reimplements them.
- MangoWM match goes by window appid `stremio-accru` (observed live in
  `mmsg get all-clients`, native Wayland); rule example lives in `--help`.

# Work Guidance

- Keep permissions broad but documented (local app, baked at compile).
- Window geometry persists to `$DATA_DIR/window.json` per upstream pattern.

# Verification

- `deno check main.ts`, `deno lint main.ts`, `deno fmt --check main.ts`
  (run from `scripts/desktop/` inside `nix develop`; the release workflow
  gates the same checks).
- Visual runs on Linux use the native player (`nix run .#app`, #58); this
  layer's window behavior is verified on the Windows/macOS release legs.

# Child DOX Index

- No child docs.
