# Purpose

- Owns CI/release automation. Current scope: `workflows/release.yml`
  (tag-triggered Windows/Linux/macOS builds, issue #46).

# Ownership

- Root AGENTS.md owns repo-wide workflow; this doc owns `.github/` contents.

# Local Contracts

- `release.yml` triggers on `v*` tags plus `workflow_dispatch`; each matrix
  leg gates on `deno check`, `deno lint`, `deno fmt --check` before compile.
- Release publishing uses `gh release create --generate-notes` only: no
  previous-release lookup, so the first tag has nothing to special-case.
- Linux/macOS legs pack tarballs, Windows packs a zip; every archive holds
  the compiled binary plus `stremio-linux.sh` and `stremio-accru.desktop`.

# Work Guidance

- Keep the workflow dependency-light (checkout, setup-deno, upload/download
  artifact only); no third-party release actions.

# Verification

- `act -l` lists the workflow; dry-plan and run the `build` job with a
  `refs/tags/v...` event payload per `skill://act-workflow-testing`.
  The `release` job needs real credentials and never runs under `act`.

# Child DOX Index

- No child docs.
