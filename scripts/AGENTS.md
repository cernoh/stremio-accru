# Purpose

- Owns the Linux launcher. Current scope: `stremio-linux.sh` (issue #38).

# Ownership

- Root AGENTS.md owns repo-wide workflow; this doc owns `scripts/` contents.

# Local Contracts

- `stremio-linux.sh` runs with `bash`, system `node`, `curl`, `jq`; `7z`
  (p7zip) optional for archive seeding. All are in `flake.nix`.
- Archive seeding is selective: anime4k contributes only `shaders/` (no
  `__MACOSX`, no root-conf clobber); thumbfast contributes only its lua +
  conf with the `portable_config/` prefix stripped, `mpv.exe` bundle left
  out, and Windows `mpv_path` commented so system mpv is used.
- Upstream contract mirrors: `src/main.cpp` flags, `src/node/server.cpp`
  (`NO_CORS=1`, `ServerStarted`), `g_webuiUrls` order per #39.
- `scripts/stremio-linux.sh --check` passes inside `nix develop`.

# Work Guidance

- Keep the script dependency-light and honest about the unported C++ embed.
- Pin upstream URLs to the `webview-windows` ref; checksums verified on fetch.

# Verification

- `nix develop --command scripts/stremio-linux.sh --check`.
- Visual run (server + browser): `nix run .#linux`.

# Child DOX Index

- No child docs.
