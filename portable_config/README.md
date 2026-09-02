# portable_config — Kai-compatible layout (synced 2026-09-02)

Mirrors [Stremio-Kai](https://github.com/allecsc/Stremio-Kai/tree/main/portable_config)
`portable_config/` so existing `mpv.conf`/`input.conf`/`script-opts/`/`scripts/`/`shaders/`
customizations carry over. Source files are GPL-3.0; Accru additions are MIT.

Synced directly from upstream raw files (not plan doc):
- `mpv.conf` — OSD, cache (900s/1GiB), subs, `gpu-next` high-quality, `[sdr]`/`[anime-sdr]` profiles (see mpv.conf header)
- `input.conf` — q/t/T/u, `` ` `` audio-cycle, F1-F4 Anime4K presets + Shift/Ctrl variants, Ctrl+1-4 thin-lines, F6-F12 (deband/deinterlace/visual/vf/sharpen/SVP) — verbatim from Kai 2026-09-02
- `script-opts/` — `notify_skip.conf`, `smart_track_selector.conf`, `svp.conf`, `thumbfast.conf` (mpv_path patched for cross-platform), `stats.conf`
- `scripts/` — `profile-manager.lua` (7.3 hybrid latch, patched to degrade cleanly when shaders/VPY missing), `thumbfast.lua`, `svp_cleanup.lua`, `reactive_vf_bypass.lua`, `notify_skip/` (main.lua + 10 modules), `smart-track-selector/` (main.lua + track_preferences.json) — kept for desktop `libmpv` compat; Rust `src-tauri/src/player/` is source of truth
- `svp_anime.vpy` / `svp_cinema.vpy` / `svp_main.vpy` (svp_main alias to svp_anime for F12 `vapoursynth="~~/svp_main.vpy"`; see `script-opts/svp.conf`)
- `shaders/` — 27 files: Anime4K presets (Clamp, Restore CNN M/S/VL/Soft, Upscale CNN x2, AutoDownscalePre, Thin, Darken, Upscale_Denoise) fetched from bloc97/Anime4K + 7 stubs (denoise1/nlmeans/hdeband/adaptive-sharpen). See `shaders/README.md`. Patched profile-manager now filters to existing files, so partial installs still work.
- `stremio-settings.ini` reference not vendored (Accru uses tauri-plugin-store).

All paths use `~~/` (MPV expands to config dir). Accru resolves `~~/` to `resourceDir/portable_config`
or portable sibling at runtime (`src-tauri/src/config/`). Tauri bundles this directory via
`src-tauri/tauri.conf.json` `bundle.resources: ["../portable_config"]`.

Cross-platform notes (vs Kai Windows-only):
- `thumbfast.conf:mpv_path` was `portable_config/mpv/mpv.exe`; Accru comments it out — system `mpv` via `PATH`/nix is used (§2.3). Uncomment on Windows portable if vendoring mpv.exe.
- VapourSynth/SVP (`svp_*.vpy`) desktop-only, opt-in (requires Python/VapourSynth/SVP); mobile uses `interpolation=yes` fallback. Guarded in patched Lua.
- Shaders gated desktop-only; missing `.glsl` gracefully skipped (filtered), not error.
