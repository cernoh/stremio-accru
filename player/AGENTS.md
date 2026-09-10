# Purpose

- Owns the native Linux desktop player (issue #58): the GTK4/WebKitGTK-6.0
  host that makes video playback run through mpv inside the app window.

# Ownership

- `scripts/AGENTS.md` owns the launcher contract (`stremio-linux.sh`); this
  doc owns `player/` and the flake `.#app`/package outputs it builds.

# Local Contracts

- One window: libmpv renders into a `GtkGLArea` underlay that fills the
  window; the transparent WebKitGTK-6.0 webview (wrapped in
  `GtkGraphicsOffload`) overlays it. WebKit HTML5 media is disabled —
  every playback goes through mpv (`vo=libmpv` + render API).
- mpv gets `config-dir` = the launcher-seeded `portable_config`
  (`~~/shaders`, thumbfast, input.conf resolve there).
- The web UI is served from a local http origin (`player/proxy.js`, node,
  port 8485) that reverse-proxies the web UI byte-for-byte. Reason: the
  current UI calls the streaming server with http://127.0.0.1:11470 URLs,
  and WebKitGTK (unlike Chromium) blocks those as mixed content from an
  https page. The upstream server `/proxy/d=` route is not used: its HTML
  rewrite 500s the current relative asset URLs.
- The web UI drives mpv over the shell transport (doc-start user script,
  `player/preload.js`): inbound `type 3` init handshake and `type 6`
  invokes (`mpv-command`, `mpv-set-prop`, `mpv-observe-prop`,
  `win-set-visibility`, `media.status`, `media.metadata`, `app-ready`,
  `quit`); outbound `type 1` events (`mpv-prop-change`, `mpv-event-ended`,
  `win-visibility-changed`). Property typing follows the official
  Stremio Linux shell tables (float/bool/string groups in
  `player_mpv.c`). Command allowlist: loadfile, sub-add, keypress, stop,
  script-message-to, cycle.
- The streaming server, portable_config seeding and the HTTPS cert stay
  owned by `../scripts/stremio-linux.sh`; the host spawns the launcher with
  `--no-browser` as a process-group child (`SERVER_IPC_KEY=LINUX`) and
  kills it on exit. TLS errors are ignored so the UI can reach the server's
  https leg (loopback cert from api.strem.io is not in the trust store).
- `preload.inc` is generated from `preload.js` by `gen_preload.sh` at build
  time (gitignored); `proxy.js` is installed to `$out/share/stremio-accru`.
- QA hooks (undocumented env, keep the normal path clean):
  `STREMIO_DEBUG_LOADFILE=<url>` starts playback through the normal mpv
  path when the UI signals ready; `STREMIO_DEBUG_HIDE_UI=1` hides the
  webview so the mpv underlay is visible for screenshots.
- `scripts/desktop/` (deno webview app, #43) is superseded on Linux by this
  host but stays for the release workflow's Windows/macOS legs (#46).

# Work Guidance

- Write original code (behavior references: official
  `Stremio/stremio-linux-shell` and community v5; both are GPL — this repo
  has no license, so no code is copied).
- Keep the host dependency-light: gtk4, webkitgtk-6.0, libmpv, libepoxy,
  dl, glib only; JSON is the hand-rolled `json.c` DOM.
- GL entry points are resolved via dlopen of libGLESv2/libGL/libEGL
  (GLVND loads its libraries locally; RTLD_DEFAULT misses them).

# Verification

- `nix build .#stremio-accru` inside the repo builds the host.
- Visual: `nix run .#app` on MangoWM — the UI loads through the local
  proxy and the log shows `transport: app ready` with no mixed-content
  blocks; playback renders below the UI (`STREMIO_DEBUG_LOADFILE` +
  `STREMIO_DEBUG_HIDE_UI` for an unobstructed screenshot).
- `stremio-accru --check` runs the launcher check and exits.

# Child DOX Index

- No child docs.
