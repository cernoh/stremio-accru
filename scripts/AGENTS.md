# Purpose

- Owns the Linux launcher. Current scope: `stremio-linux.sh` (issue #38).

# Ownership

- Root AGENTS.md owns repo-wide workflow; this doc owns `scripts/` contents.

# Local Contracts

- `stremio-linux.sh` runs with `bash`, system `node`, `curl`, `jq`; `7z`
  (p7zip) optional for archive seeding. All are in `flake.nix`.
- Upstream contract mirrors: `src/main.cpp` flags, `src/node/server.cpp`
  (`NO_CORS=1`, `ServerStarted`), `g_webuiUrls` order per #39.
- `scripts/stremio-linux.sh --check` passes inside `nix develop`.

# Work Guidance

- Keep the script dependency-light and honest about the unported C++ embed.
- Pin upstream URLs to the `webview-windows` ref; checksums verified on fetch.

# Verification

- `nix develop --command scripts/stremio-linux.sh --check`.

# Child DOX Index

- No child docs.
