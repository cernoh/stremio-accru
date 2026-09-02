# portable_config — Compat Layout

Mirrors Kai's `portable_config/` so existing `mpv.conf`/`input.conf`/`script-opts/`/`scripts/`/`shaders/` customizations carry over.

- `mpv.conf`, `input.conf` — baseline + keybinds
- `script-opts/` — tunables (svp, thumbfast, etc.)
- `scripts/` — Lua (profile-manager, notify_skip, smart-track-selector, thumbfast) — kept for desktop compat; Rust is source of truth
- `shaders/` — GLSL Anime4K presets
- `svp_*.vpy` — VapourSynth (desktop, opt-in)

All paths use `~~/` (MPV expands to config dir). Accru resolves `~~/` to `resourceDir/portable_config` or portable sibling at runtime.
