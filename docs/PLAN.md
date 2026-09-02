# Stremio Accru — Platform-Agnostic Stremio-Kai Plan

> Goal: replicate Stremio-Kai's premium experience (portable MPV, automation,
> modern UI) on **Windows, macOS, Linux, Android, iOS** from a single codebase.

Date: 2026-09-02 | Status: Implemented — M0–M5 merged to `main` @ `04372e2` (CI green). See README for current head. | Sources: `stremio-linux-shell` (GTK4+WebKitGTK+libmpv Rust), `stremio-core` (Rust Elm-arch), `Stremio-Kai` (Zaarrg/community-v5 overlay, `portable_config/`)

## 1. Research Synthesis

### stremio-linux-shell (`Stremio/stremio-linux-shell`, 141★, `main`, Rust/GPL-3.0)

- **Shell:** GTK4 + libadwaita + WebKitGTK6 + libmpv2 + tokio.
  `src/app/{window,webview,video,config}`. `Video` wraps `GLArea` → `libmpv2`
  (`observe_mpv_property`/`send_mpv_command`/`set_mpv_property`). Webview loads
  `stremio-web` via `server.rs` (local HTTP server). IPC via `app/ipc`.
- **Platform coupling:** Linux-only (epoxy/libepoxy, Flatpak, `dirs::data_dir`).
  No mobile. Build: Cargo + Flatpak.
- **Takeaway:** Thin Rust shell around `stremio-core` + WebView + libmpv.
  Replace GTK/WebKit with cross-platform shell.

### stremio-core (`Stremio/stremio-core`, 2302★, `development`, Rust/MIT)

- **Architecture:** Elm-inspired `Runtime → Msg → Models → Effects → Env`. UI
  dispatches `Action`; models (`Ctx`, `Player`, `CatalogWithFilters`,
  `MetaDetails`, `StreamingServer`, `Calendar`, `Library`) update and return
  `Effects`; `Env` trait abstracts `fetch/storage/exec/time`. Any platform
  implements `Env`.
- **Crates:** `src/types` (addon protocol, meta, streams), `src/models`,
  `src/runtime`, `src/addon_transport`, `src/deep_links`, `stremio-core-web`
  (WASM bridge → npm `@stremio/stremio-core-web`, Web Worker for `stremio-web`),
  `stremio-derive` (`#[derive(Model)]`).
- **Takeaway:** Reuse via `stremio-core-web` WASM in frontend **or** native Rust
  `stremio-core` in Tauri backend. Either way, Env is the seam.

### Stremio-Kai (`allecsc/Stremio-Kai` → parent `Zaarrg/stremio-community-v5`, 628★, JS/GPL-3.0, Windows-only)

- **Nature:** Not a C++ fork — **config overlay distribution**. Repo is only
  `portable_config/` + `docs/` + `README`. Inherits Qt6/WebView2 shell (Windows)
  from `stremio-community-v5`; all Kai value is vendored MPV + Lua + GLSL +
  WebMods injected into WebView.
- **Portability:** `portable_config/` relocatable (`~~/` = exe-adjacent config
  dir), `stremio-settings.ini` local instead of `%APPDATA%`, no registry.
  Release: manual 7z (383 MB) + Setup.exe + Update diff + Winget. No CI.
- **MPV (`mpv.conf`):** `vo=gpu-next`, `gpu-api=auto`, `profile=high-quality`,
  `cache 900s/1GiB`, `fruit dither 10-bit + temporal`, `spline36/lanczos`,
  `target-prim/contrast=auto`, profiles `[sdr]`/`[anime-sdr]` — dynamic layers
  by `profile-manager.lua`.
- **Scripts:** `profile-manager.lua` (33KB, hybrid latch `video-params` +
  `anime-metadata` bridge, HDR passthrough/tonemap, Anime4K presets
  `optimized/fast/hq`, `hqdn3d`/`bwdif`+`vapoursynth svp_*.vpy`,
  Kai/Vivid/Original, Night/Voice audio, SVP queue), `notify_skip`
  (IntroDB→chapters→filter), `smart-track-selector` (forced subs, rejection
  lists, embedded>external), `thumbfast.lua`. `input.conf` F1-F12 preset binds.
  Shaders `Anime4K_*`, `svp_anime.vpy`/`svp_cinema.vpy` (SVP+MVTools, 48/60fps).
- **WebMods (`webmods/`):** Injected JS/CSS: `UI/Hero Banner` (8s rotation,
  6-day cache, Snoak/Cinemeta/MDBList/Jikan with 3-proxy fallback), `Metadata`
  (TMDB/MDBList 7-day TTL, rate 5/s, localized IMD
  b/TMDB/Trakt/MAL/AniList/Kitsu, cast photos, network badges), `Theme` (OLED
  pure-black `#000000` toggle via `oled-theme-toggle.js`),
  `Utilities/navigation.js` (hidden sidebar/search on hover, update checker
  polling `api.github.com/repos/allecsc/Stremio-Kai/releases/latest`),
  `Settings/mpv-settings.js` (67KB, instant apply via `mpv-bridge.js` →
  `mp.register_script_message("anime-metadata")`).
- **Settings:** `mpv-settings.js` + `enhanced-metadata.js` +
  `oled-theme-toggle.js` + `custom-shortcuts.js` + `auto-fullscreen.js`. Instant
  apply, First-Time Wizard, private TMDB/MDBList keys. **No more editing
  `.conf`**.
- **Gap for portability:** `~~/` paths already relative (good), but
  VapourSynth/Python/SVP binaries platform-specific, WebMods assume WebView2
  injection point, shaders assume `gpu-next`. Must abstract.

## 2. Architecture Decision

#### Why not fork `stremio-linux-shell`?

Too coupled to GTK/WebKitGTK/libepoxy/Flatpak. Tauri gives platform
abstraction + updater + bundler + portable mode for free.

**Frontend via Deno:** Vite/SvelteKit run with **Deno 2** as primary
(`deno.json` tasks, `npm:` specifiers, `nodeModulesDir:auto`, `unstable byonm`
for Tauri interop). `package.json` retained for npm fallback/CI. CI and Tauri
`beforeDevCommand` use `deno task dev/build`.

### 2.2 High-Level Layers

```
┌─────────────────────────────────────────────────┐
│ Frontend (TypeScript + Svelte/Solid + Vite)     │
│  Hero Banner · Metadata Panel · Details ·       │
│  Hidden Nav · OLED · Settings Wizard · Player UI│
│  ← stremio-core-web WASM or Tauri invoke ─────→ │
├─────────────────────────────────────────────────┤
│ Tauri Backend (Rust)                            │
│  stremio-core Runtime (Env impl: fetch/storage) │
│  Player Abstraction (trait)                     │
│   ├─ DesktopPlayer → libmpv2 + gpu-next + Lua* │
│   └─ MobilePlayer → libmpv-android / mpvkit-ios│
│      (fallback: ExoPlayer/AVPlayer if needed)   │
│  Portable Config Layer (~~/ equivalent)         │
│  Addon/StreamingServer bridge                   │
│  Updater (tauri-plugin-updater)                 │
├─────────────────────────────────────────────────┤
│ Assets (bundled, relocatable)                   │
│  mpv.conf + input.conf + script-opts/           │
│  scripts/*.lua + shaders/*.glsl + svp_*.vpy     │
│  webmods → migrated to frontend components      │
└─────────────────────────────────────────────────┘
* Lua scripts ported to Rust player controller; keep .lua for desktop compat if desired.
```

**IPC:** Frontend ↔ Tauri via `invoke`/`emit` (commands + events). Player events
(`property-changed`, `playback-ended`) forwarded as Tauri events to frontend.
Settings persisted via `tauri-plugin-store` + portable INI compat.

### 2.3 Portability Layer (Zero-Config)

- **Detection:** At launch, check if `portable_config/` (or `./accru_portable/`
  ) sibling to binary exists **and** writable. If yes → portable mode (all data
  in `portable_data/`). Else → XDG/AppData
  (`dirs::data_dir/join("stremio-accru")`). Env var `ACCRU_PORTABLE=1` forces
  portable.
- **Paths:** No absolute paths; all asset references relative to
  `resourceDir`/`configDir` (Tauri `path::resourceDir`). MPV `~~/` emulated by
  resolving `resourceDir/portable_config/`. Cache dir `cache/shaders` relative.
- **Bundling:** Tauri `resources` includes `portable_config/` + shaders. On
  first run, extract/update from resources to data dir (diff-merge via version
  stamp). USB/extract-and-play works on Win/Mac/Linux; Android/iOS sandbox is
  not portable (falls back to app data — expected).
- **Installer vs Portable:** Tauri `bundle.targets` = `nsis`/`msi` (Win),
  `app`/`dmg` (Mac), `appImage`/`deb` (Linux). Portable zip produced by
  `tauri build` + manual 7z (like Kai). Update via `tauri-plugin-updater`
  polling GitHub Releases (auto for installer, notify for portable).

### 2.4 Player Abstraction (MPV Everywhere, Native Fallback)

```rust
#[async_trait]
trait PlayerBackend {
  fn load(&self, url: &str, opts: LoadOpts) -> Result<()>;
  fn set_property(&self, key: &str, val: serde_json::Value) -> Result<()>;
  fn observe(&self, key: &str) -> Result<()>;
  fn command(&self, cmd: &str, args: &[&str]) -> Result<()>;
  fn set_shader_preset(&self, preset: Anime4KPreset) -> Result<()>;
  fn set_visual_profile(&self, profile: VisualProfile) -> Result<()>;
  fn set_audio_preset(&self, preset: AudioPreset) -> Result<()>;
  // events → frontend
}
enum VisualProfile { Kai, Vivid, Original }
enum Anime4KPreset { Optimized, Fast, HQ, Off }
enum AudioPreset { Off, Night, Voice }
```

- **Desktop:** `DesktopPlayer` wraps `libmpv2` (as linux-shell does: `GLArea`
  replaced by Tauri `webview` overlay + `libmpv` `create_render_context` with
  `OpenGL`/`Vulkan`). Reuse `profile-manager.lua` logic in Rust
  (`player/profiles.rs`) + keep Lua support via `mpv` `script` loading for
  plugin compat. `input.conf` → Tauri `globalShortcut` + frontend key handler.
- **Android:** Build `libmpv` via NDK (prebuilt `mpv-android` AAR), JNI bridge;
  `vo=gpu` via `libplacebo`/GL. Fallback to `ExoPlayer` if `libmpv` unavailable
  on device. SVP/Anime4K shaders disabled on mobile (GPU constraints) — mark as
  desktop-only in settings UI.
- **iOS:** `mpvkit` (libmpv + Metal `vo=gpu-next` via MoltenVK) or `AVPlayer`
  fallback. App Store note: libmpv is GPL — iOS distribution must provide
  source; fallback to `AVPlayer` for App Store build variant.
- **HDR:** `gpu-next` `target-colorspace-hint`, `tone-mapping=bt.2446a`,
  `gamut-mapping=perceptual` — exposed as toggle; non-HDR displays auto tonemap
  (as Kai).
- **Shaders:** GLSL portable; bundle in `resources/shaders/`. Anime4K toggled
  via `glsl-shaders` list.
- **SVP:** VapourSynth+SVP on desktop only (opt-in, requires Python/VapourSynth
  runtime — document, package optionally). Mobile uses `interpolation=yes`
  (`vo=gpu` MEMC) as lightweight alternative.

### 2.5 Kai Feature Mapping

| Kai Feature                                                                    | Accru Location                                                                                                          | Mobile Degradation       |
| ------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------- | ------------------------ |
| Dynamic Hero Banner + custom MDBList sources                                   | `src/frontend/features/hero/` (Svelte) + Rust cache/service (Snoak/Cinemeta/MDBList/Jikan, 6-day cache, proxy fallback) | Same                     |
| Metadata Panel (hover, localized, 6 rating sources)                            | `features/metadata/` + TMDB/MDBList fetchers, 7-day TTL, rate limiter                                                   | Tap instead of hover     |
| Enhanced Details (cast photos, networks, episodes)                             | `features/details/`                                                                                                     | Same                     |
| Hidden Nav (sidebar/search auto-hide)                                          | CSS `hover` + `navigation.ts` lifecycle                                                                                 | Drawer (swipe)           |
| OLED Pure Black                                                                | CSS vars `theme/oled.css` + `store` toggle                                                                              | Same (AMOLED)            |
| Auto Fullscreen                                                                | Settings → launch `window.setFullscreen`                                                                                | n/a                      |
| Update System                                                                  | `tauri-plugin-updater` + GitHub Releases                                                                                | Store update             |
| Skip Opening (IntroDB/chapters/filter)                                         | `player/skip/` Rust + frontend toast (IntroDB priority, confirmation)                                                   | Same (IntroDB)           |
| Smart Track Selector (forced override, rejection)                              | `player/tracks/` Rust, `track_preferences.json` compat                                                                  | Same                     |
| Hover-Seek Thumbnails (thumbfast)                                              | `player/thumbnails/` — backend generates via `ffmpeg`/`mpv` thumb, frontend canvas                                      | Same (touch seek)        |
| Hi-Fi Audio (Cinema/Anime/Night)                                               | `player/audio.rs` `af lavfi loudnorm` → Night/Voice EQ presets                                                          | Night/Voice only         |
| Visual Profiles Kai/Vivid/Original                                             | `player/visual.rs` `contrast/brightness/saturation/gamma` + ICC                                                         | Same                     |
| Cinematic HDR passthrough                                                      | `player/hdr.rs`                                                                                                         | Toggle disabled (no HDR) |
| Daily Schedule (anime)                                                         | Hero Banner daily tab, Jikan `schedules`                                                                                | Same                     |
| Anime4K Upscaling                                                              | `shaders/` + `player/shaders.rs`, presets Optimized/Fast/HQ toggle                                                      | Desktop only (gated)     |
| SVP Interpolation 48/60fps                                                     | `player/svp.rs` + `svp_*.vpy`, toggle                                                                                   | Desktop only (gated)     |
| Settings Overhaul (no .conf editing, wizard, TMDB/MDBList keys, instant apply) | `features/settings/` Svelte panels + `tauri-plugin-store`, `mpv-bridge` invoke → instant `set_property`                 | Same, mobile-gated opts  |
| First-Time Wizard                                                              | `features/onboarding/`                                                                                                  | Same                     |
| Portable/Instant changes                                                       | Portable layer + Tauri store watch                                                                                      | n/a (sandbox)            |

### 2.6 stremio-core Integration

- **Preferred:** Rust-native `stremio-core` as Tauri dependency (`Cargo.toml`
  `stremio-core` git `development`). Implement `Env` (fetch via `reqwest`,
  storage via `tauri-plugin-store`/portable file, exec via `tokio`, time via
  `chrono`). Expose `Runtime` commands: `dispatch_action`, `get_state`,
  `subscribe`.
- **Alternative path:** `stremio-core-web` WASM in frontend Web Worker for quick
  parity with `stremio-web`. Keep option; but Rust-native is faster and
  mobile-friendlier.
- Streaming: reuse `stremio-core` `StreamingServer` model + `addon_transport`.
  Player loads resolved stream URL.

### 2.7 Monorepo Skeleton

```
stremio-accru/
  src-tauri/            # Rust backend (Tauri)
    Cargo.toml
    tauri.conf.json
    src/
      main.rs
      lib.rs
      core/             # stremio-core Runtime + Env impl
      player/           # trait + desktop/mobile + profiles/audio/hdr/shaders/svp/tracks/skip/thumbnails
      config/           # portable vs installed, paths, migration
      updater/
  src/                  # Frontend (SvelteKit + Vite + TS)
    lib/
      features/
        hero/ catalog-service cache api-selector
        metadata/ fetchers services
        details/
        navigation/
        settings/ mpv-settings enhanced-metadata oled theme
        onboarding/
        player/ controls skip toast tracks
      stores/
      theme/
    routes/
  portable_config/      # Compat overlay (mpv.conf, input.conf, scripts, shaders, script-opts, svp_*.vpy)
    (symlink or copy of Kai layout, adapted)
  resources/            # Tauri bundle resources (shaders, scripts)
  docs/PLAN.md
  .github/workflows/ci.yml
  .github/ISSUE_TEMPLATE/
```

## 3. Phases & Milestones

1. **M0 Skeleton** — Tauri init, CI, issue board, portable skeleton,
   `docs/PLAN.md`. Establish `main` + branch protection.
2. **M1 Player Core** — `PlayerBackend` trait, desktop `libmpv2` embed, basic
   play/pause/seek/volume, `mpv.conf` baseline, input binds.
3. **M2 Core + Streaming** — `stremio-core` Env, addon/catalog, stream
   resolution, playback via player.
4. **M3 UI/UX** — Hero Banner (caching, MDBList), Metadata hover, Details,
   Hidden Nav, OLED, Auto Fullscreen.
5. **M4 Automation & Presets** — Skip Opening, Smart Track Selector, thumbnails,
   Hi-Fi Audio, Visual Profiles, HDR, Anime4K, SVP (desktop gated).
6. **M5 Settings & Portability** — Settings UI overhaul (mpv/settings, keys,
   instant apply), wizard, portable detection + migration, updater, packaging
   (portable 7z + installer + Winget note).
7. **M6 Mobile** — Android/iOS shells, mobile player fallback, theme/touch
   adaptations, store metadata.

Each milestone gets GitHub issues + PR per issue (stacked).

## 4. Risks & Mitigations

- **iOS libmpv / GPL-App-Store:** gate behind `#[cfg]` + AVPlayer fallback
  build.
- **SVP/VapourSynth bundling:** large (~200MB), opt-in download; not bundled by
  default, document manual install.
- **WebMods tech debt:** don't port JS injection; reimplement as Svelte
  components (clean).
- **stremio-core API churn:** pin `Cargo.lock`, follow `development` branch, CI
  checks.
- **Tauri mobile maturity:** keep `stremio-core-web` WASM path as hedge;
  fallback to Capacitor if Tauri mobile blocks.

## 5. Next Steps (Immediate)

1. Init repo, `src-tauri` + frontend scaffold, `portable_config` compat, push to
   GitHub, create issues (this doc is the source).
2. PR #1: Scaffold + CI (this plan).
3. PR #2+: Player → Core → UI → Presets → Settings → Mobile in order, each
   `Closes #N`.
