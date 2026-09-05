# stremio-accru

Stremio Community on Linux. Upstream `Zaarrg/stremio-community-v5`
(`webview-windows`) is Win32 + WebView2 only; this repo ships the
**launcher now, GTK later** (scope: `docs/linux-scope.md`).

## Features

- **Linux launcher** (`scripts/stremio-linux.sh`, #38): seeds
  `portable_config` (settings, mpv configs, anime4k/thumbfast assets)
  under the XDG data dir, fetches versioned `server.js` via the
  version-baseline chain, runs it with system node + `NO_CORS=1`, opens
  the web UI (`web.stremio.com` first). Flags: `--webui-url=`,
  `--streaming-server-disabled`, `--check`, `--help`. Single-instance
  lock. Provisions the server HTTPS cert so the web UI detects the
  server. `scripts/stremio-linux.sh --check` verifies the install.
- **Standalone desktop app** (`scripts/desktop/main.ts`, #43): Deno
  desktop window (webview backend) supervising the launcher server.
  MangoWM needs `WEBKIT_DISABLE_DMABUF_RENDERER=1` and
  `GIO_EXTRA_MODULES` (set by the wrapper); the wrapper self-heals the
  downloaded webview backend after re-downloads. Window geometry
  persists per upstream pattern.
- **Tag releases** (`.github/workflows/release.yml`, #46): pushing a
  `v*` tag compiles the app on Linux, Windows, and macOS runners and
  publishes the archives to the tag's GitHub Release.

## Download and run

Prebuilt binaries live on the
[Releases page](https://github.com/cernoh/stremio-accru/releases)
(first release:
[v0.1.0](https://github.com/cernoh/stremio-accru/releases/tag/v0.1.0)).

```sh
# Linux
gh release download v0.1.0 -p 'stremio-accru-linux.tar.gz' \
  -R cernoh/stremio-accru
tar -xzf stremio-accru-linux.tar.gz
./stremio-accru-linux --check   # then run without --check

# macOS
gh release download v0.1.0 -p 'stremio-accru-macos.tar.gz' \
  -R cernoh/stremio-accru
tar -xzf stremio-accru-macos.tar.gz
./stremio-accru-macos --check

# Windows (PowerShell)
gh release download v0.1.0 -p 'stremio-accru-windows.zip' `
  -R cernoh/stremio-accru
Expand-Archive stremio-accru-windows.zip .
.\stremio-accru-windows.exe --check
```

Each archive holds the compiled binary plus `stremio-linux.sh` and
`stremio-accru.desktop`. The binary needs system `node` and `mpv`
for the streaming server and playback.

NixOS note: release binaries target generic Linux and do not run
under the NixOS stub-ld. On NixOS use the flake instead (patchelf
handling stays in `flake.nix`):

```sh
nix run .#app      # desktop app (default)
nix run .#linux    # launcher visual run
nix develop --command scripts/stremio-linux.sh --check
```

## Develop

```sh
nix develop   # or direnv (`use flake` via .envrc)
deno check scripts/desktop/main.ts
deno lint scripts/desktop/main.ts
deno fmt --check scripts/desktop/main.ts
```
