# Stremio Accru

Platform-agnostic Stremio-Kai — Windows, macOS, Linux, Android, iOS. Single codebase, MPV everywhere, zero-config portability.

> **Status:** Skeleton / M0. See [`docs/PLAN.md`](docs/PLAN.md) for architecture, research, and milestones.

## Stack

- **Shell:** [Tauri v2](https://tauri.app) (Rust backend + WebView frontend)
- **Core:** [`stremio-core`](https://github.com/Stremio/stremio-core) (Rust `Env` impl) + `stremio-core-web` WASM fallback
- **Frontend:** SvelteKit + Vite + TypeScript
- **Player:** `libmpv2` on desktop (`gpu-next`, GLSL shaders), mobile `libmpv` NDK / `mpvkit` / native fallback, abstracted via `PlayerBackend` trait

## Key Features (ported from Kai)

- Zero-config portability — extract-and-play, relative `~~/` paths, writable `portable_data/`
- Dynamic Hero Banner (MDBList, Cinemeta, Jikan, 6-day cache), custom catalog sources
- Metadata panel (TMDB/MDBList, 6 rating sources, localized), enhanced details (cast/crew, networks)
- Hidden navigation (hover/drawer), OLED pure-black, auto-fullscreen, update system
- Skip Opening (IntroDB > chapters > filter), Smart Track Selector (forced override), hover thumbnails
- Hi-Fi Audio (Night/Voice), Visual profiles (Kai/Vivid/Original), HDR passthrough + tonemapping
- Anime4K (Optimized/Fast/HQ), SVP interpolation (desktop, toggleable, opt-in)

All settings in UI, instant apply, first-time wizard. See `docs/PLAN.md` §2.5 for full map + mobile degradation.

## Quick Start

### Nix (recommended on NixOS)

```bash
direnv allow        # or: nix develop
npm install
npm run tauri dev      # desktop
cargo check --manifest-path src-tauri/Cargo.toml
```

The flake (`flake.nix`) provides Rust stable (+ `wasm32-unknown-unknown`), `cargo-tauri`, Node 20, and Tauri Linux deps (webkitgtk 4.1, libsoup 3, gtk3, mpv, appimagekit). `WEBKIT_DISABLE_DMABUF_RENDERER=1` is set for NixOS webkit.

### Non-Nix

```bash
# prerequisites: Rust stable, Node 20+, Tauri deps (https://tauri.app/start/prerequisites/)
npm install
npm run tauri dev      # desktop
npm run tauri android dev
npm run tauri ios dev
```

Portable mode: place `portable_config/` next to binary (or set `ACCRU_PORTABLE=1`). See `portable_config/README.md`.

## Structure

```
src-tauri/        # Rust backend (core, player, config, updater)
src/              # Frontend (SvelteKit, features/*, theme, stores)
portable_config/  # MPV + scripts + shaders compat (relative paths)
resources/        # Tauri bundle resources
docs/PLAN.md      # Architecture & research
.github/          # CI, issue/PR templates
```

## Research Sources

- `https://github.com/Stremio/stremio-linux-shell` (GTK4+WebKitGTK+libmpv)
- `https://github.com/Stremio/stremio-core` (Elm-architecture Rust core)
- `https://github.com/allecsc/Stremio-Kai` (Zaarrg/community-v5 overlay, `portable_config/`)

## License

GPL-3.0-only — same as `stremio-linux-shell` / Kai.

## Contributing

Issues are the work board — each milestone is an issue. PRs must reference `Closes #N`.
