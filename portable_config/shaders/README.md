# shaders — GLSL presets (bundled, matches Kai)

Kai's `input.conf` and `profile-manager.lua` reference `~~/shaders/*.glsl`. This
directory now bundles 27 files so presets reproduce Kai without silent
fallbacks.

Sources (all fetched 2026-09-02):

- Anime4K (bloc97/Anime4K, MIT): 19 files Restore: Clamp_Highlights, Restore_CNN
  M/S/VL, Soft M/S/VL Upscale: CNN_x2 M/S/VL, AutoDownscalePre x2/x4,
  Upscale_Denoise CNN_x2_M Experimental: Thin Fast/HQ/VeryFast, Darken
  Fast/HQ/VeryFast
- Community (real, not Kai-custom stubs): nlmeans.glsl (AN3223/dotfiles, 73KB),
  hdeband.glsl (AN3223, 6.5KB), adaptive-sharpen.glsl (deus0ww/mpv-conf igv,
  12KB), denoise1/2/3.glsl (AN3223 nlmeans_light_temporal variants, 72-73KB),
  sharpen_denoise.glsl (AN3223 nlmeans_sharpen_denoise, 73KB)

All files are valid GLSL with HOOK directives; patched `profile-manager.lua`
still filters to existing files as safety, but with this bundle no filtering is
needed — the anime pipeline (denoise → Anime4K → Thin) runs at Kai fidelity
instead of silently skipping stages.
