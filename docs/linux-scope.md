# Linux port scope — v1 decision (issue #36)

Upstream `Zaarrg/stremio-community-v5` (`webview-windows` @ `3e96a6f`) is
Win32 + WebView2 only. Linux v1 ships the **launcher now, GTK later**.

## v1: `scripts/stremio-linux.sh` (#38)

Runs Stremio Community on Linux without the Win32 C++ shell:

- Seeds `portable_config` (`stremio-settings.ini`, mpv base configs, anime4k /
  thumbfast assets) under the XDG data dir.
- Fetches versioned `server.js` when missing and runs it with system node and
  `NO_CORS=1` (mirrors `src/node/server.cpp`: env, stdout pipe,
  `ServerStarted` contract).
- Opens the web UI URL; flags mirror upstream `src/main.cpp`:
  `--webui-url=` (prepends), `--streaming-server-disabled`,
  `--autoupdater-endpoint=` (version-baseline override).
- Single-instance lock file; honest `--help` stating the C++ embed is
  not ported.

## Maintainer answers consumed (#39)

1. Web UI order: `web.stremio.com` first, then `stremio.zarg.me`, then the
   `zaarrg.github.io` fallback (upstream `g_webuiUrls` reordered).
2. `server.js`: follow the latest S3 build via the version-baseline chain
   (`version.json` → `version-details.json` → `files["server.js"]`), not a
   hardcoded version. Canonical form:
   `https://dl.strem.io/server/<version>/desktop/server.js`.
3. `zaarrg` stays a read-only remote for cherry-picks; `origin` is the only
   push target.
4. nixpkgs pin: `nixos-unstable`.
5. `nix-direnv` is available; `.envrc` keeps `use flake`.
6. Linux updater v1: minimal version check (cached `server.js` checksum /
   version against baseline, re-fetch on mismatch).

## Portable vs Windows-only partition

From pre-reset analysis of upstream @ `3e96a6f` (paths refer to that tree):

Portable — reused on Linux with platform shims:

- Node server contract: `server.js` + `NO_CORS=1`, stdout pipe, `ServerStarted`
  event (`src/node/server.cpp`) — `CreateProcess`/Job objects become fork+exec.
- `portable_config` assets: `utils/stremio/stremio-settings.ini`,
  `utils/mpv/anime4k/portable_config/{mpv.conf,input.conf}`,
  `utils/mpv/anime4k/anime4k-High-end.zip`, `utils/mpv/thumbfast/thumbfast.7z`.
- Updater verify logic (curl + openssl + sha256) minus NSIS paths.
- Web UI URL list + `--webui-url=` prepend semantics.

Windows-only — replaced, not ifdeffed:

- WebView2 COM (`src/webview`, `globals.h`), `WndProc`/main window, GDI+
  splash, tray, named-mutex single instance, media hotkey, DPI APIs.
- `discord-rpc` win64-static lib, `utils/windows` binaries, NSIS installer,
  `deploy_windows.js`, CMake WIN32 exe.

## Out of scope for v1

- Native GTK/WebKit host (webkitgtk 6.0 candidate pinned in `flake.nix`
  for later) — follow-up issue, not this one.
- Windows installer/docs work (#11: showcase, winget, disclaimer) — deferred,
  unrelated to the Linux port.
- Full auto-updater (signatures, partial `server.js` patch keys) — v1 checks
  version/checksum only.

## Rule

Every Linux-port PR references this issue (#36) in its body.
