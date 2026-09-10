# Purpose

- Owns CI/release automation. Current scope: `workflows/release.yml`
  (tag-triggered Windows/Linux/macOS builds, issue #46).

# Ownership

- Root AGENTS.md owns repo-wide workflow; this doc owns `.github/` contents.

# Local Contracts

- `release.yml` triggers on `v*` tags plus `workflow_dispatch`; the
  Linux and Windows/macOS legs each gate on `deno check`, `deno lint`,
  `deno fmt --check` before compiling.
- Linux artifact is built with nix from the repo flake:
  `nix build .#stremio-accru-release` (`releasePkg` in `flake.nix`,
  issue #56) — a `deno compile` derivation with NO NixOS patchelf, so the
  binary runs on stock glibc distros. The NixOS-wrapped
  `.#stremio-accru` package is only for `nix run .#app` on NixOS.
- Windows/macOS legs compile with `deno compile` on their native runners.
- Release publishing uses `gh release create --generate-notes` only: no
  previous-release lookup, so the first tag has nothing to special-case.
- Linux/macOS legs pack tarballs, Windows packs a zip; every archive holds
  the compiled binary plus `stremio-linux.sh` and `stremio-accru.desktop`.
- Root `.gitattributes` pins LF for sources: Windows CRLF checkouts fail
  `deno fmt --check` and break `#!` scripts. Matrix is `fail-fast: false`
  so one OS never cancels the others. The build job defaults
  `run.shell` to `bash` (git-bash on windows-latest); only the zip pack
  step overrides to `pwsh` for `Compress-Archive`.

# Work Guidance

- Keep the workflow dependency-light (checkout, setup-deno,
  install-nix-action, upload/download artifact only); no third-party
  release actions.

# Verification

- `act -l` lists the workflow; dry-plan and run the `build-nix`/`build`
  jobs with a `refs/tags/v...` event payload per
  `skill://act-workflow-testing`. The `release` job needs real
  credentials and never runs under `act`.
- `nix build .#stremio-accru-release` must succeed and output a
  dynamically linked binary without a nix-store interpreter/rpath
  (`patchelf --print-interpreter` on the result shows the loader only if
  patched; `file` must not reference the nix store).

# Child DOX Index

- No child docs.
