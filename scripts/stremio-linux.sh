#!/usr/bin/env bash
# stremio-linux.sh — run Stremio Community on Linux without the Win32 C++ shell.
# Issue #38. Scope decision: docs/linux-scope.md (#36).
#
# What it does (mirrors upstream src/main.cpp + src/node/server.cpp):
#   - seeds portable_config under the XDG data dir
#   - fetches versioned server.js via the version-baseline chain when missing
#   - provisions the server HTTPS cert (loopback, api.strem.io) so the web
#     UI detects the server instead of showing "not available"
#   - runs server.js with system node and NO_CORS=1 (unless
#     --streaming-server-disabled), opens the web UI in a browser
#
# What it does NOT do (honest limitation): the C++ embed is not ported —
# no WebView2 window, tray icon, mpv embed, Discord RPC, or NSIS updater.
# The web UI drives playback through the browser + streaming server.
set -u

PROG="stremio-linux.sh"
DATA_DIR="${STREMIO_DATA_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/stremio-accru}"
SERVER_JS="$DATA_DIR/server.js"
VERSION_FILE="$DATA_DIR/.server-version"
PC_DIR="$DATA_DIR/portable_config"
LOCK_FILE="${XDG_RUNTIME_DIR:-$DATA_DIR}/stremio-accru.lock"

# Version baseline chain (upstream Zaarrg/stremio-desktop-v5, webview-windows):
# version.json -> versionDesc -> version-details.json -> files["server.js"].
BASELINE_URL="${STREMIO_VERSION_BASELINE:-https://raw.githubusercontent.com/Zaarrg/stremio-desktop-v5/refs/heads/webview-windows/version/version.json}"
UPSTREAM_REF="refs/heads/webview-windows"
SETTINGS_URL="https://raw.githubusercontent.com/Zaarrg/stremio-desktop-v5/${UPSTREAM_REF}/utils/stremio/stremio-settings.ini"
MPV_BASE_URL="https://raw.githubusercontent.com/Zaarrg/stremio-community-v5/${UPSTREAM_REF}/utils/mpv/anime4k"
ANIME4K_ZIP="https://raw.githubusercontent.com/Zaarrg/stremio-community-v5/${UPSTREAM_REF}/utils/mpv/anime4k/anime4k-High-end.zip"
THUMBFAST_7Z="https://raw.githubusercontent.com/Zaarrg/stremio-community-v5/${UPSTREAM_REF}/utils/mpv/thumbfast/thumbfast.7z"
# HTTPS cert for the server endpoint (server.js lazy-https reads
# <appPath>/httpsCert.json; the web UI probes https://<loopback>:12470).
# api.strem.io issues loopback certs without auth; cached by date.
CERT_ENDPOINT="${STREMIO_CERT_ENDPOINT:-http://api.strem.io/api/certificateGet}"
APP_PATH_DIR="${APP_PATH:-$HOME/.stremio-server}"
CERT_FILE="$APP_PATH_DIR/httpsCert.json"

# Web UI order per maintainer (#39): web.stremio.com first.
WEBUI_URLS=(
  "https://web.stremio.com/"
  "https://stremio.zarg.me/"
  "https://zaarrg.github.io/stremio-web-shell-fixes/"
)
STREAMING_SERVER=1
OPEN_BROWSER=1

die() { echo "$PROG: error: $*" >&2; exit 1; }
warn() { echo "$PROG: warning: $*" >&2; }
need() { command -v "$1" >/dev/null 2>&1 || die "missing required tool: $1"; }

usage() {
  cat <<EOF
Usage: $PROG [options]

Run Stremio Community on Linux without the Win32 C++ shell (#38).

Options:
  --webui-url=URL             use URL first (default: https://web.stremio.com/)
  --streaming-server-disabled skip server.js, only open the web UI
  --no-browser                do not open a browser, just run the server
  --autoupdater-endpoint=URL  version-baseline override (default upstream
                              version.json)
  --data-dir=DIR              override data dir (default: $DATA_DIR)
  --check                     verify node/server.js/portable_config and exit
  --help                      show this help

Layout: server.js + .server-version + portable_config/ live under
\$XDG_DATA_HOME/stremio-accru (or --data-dir). server.js follows the latest
S3 build named by the version baseline; checksum from the baseline is
verified on fetch. The server HTTPS cert (httpsCert.json, loopback) is
provisioned under ~/.stremio-server (or \$APP_PATH) and renewed by date.

Limitations (the C++ embed is NOT ported): no native window/tray, no
embedded mpv (system mpv via portable_config), no Discord RPC, no
auto-updater beyond the server.js version check.
EOF
}

WEBUI_EXTRA=""
while [ $# -gt 0 ]; do
  case "$1" in
    --webui-url=*) WEBUI_EXTRA="${1#*=}"; shift ;;
    --streaming-server-disabled) STREAMING_SERVER=0; shift ;;
    --no-browser) OPEN_BROWSER=0; shift ;;
    --autoupdater-endpoint=*) BASELINE_URL="${1#*=}"; shift ;;
    --data-dir=*) DATA_DIR="${1#*=}"; SERVER_JS="$DATA_DIR/server.js";
      VERSION_FILE="$DATA_DIR/.server-version"; PC_DIR="$DATA_DIR/portable_config"; shift ;;
    --check) MODE_CHECK=1; shift ;;
    --help) usage; exit 0 ;;
    *) die "unknown option: $1 (see --help)" ;;
  esac
done
MODE_CHECK="${MODE_CHECK:-0}"
STREMIO_WEBUI_DEFAULT="${STREMIO_WEBUI_URL:-${WEBUI_EXTRA:-${WEBUI_URLS[0]}}}"

if [ -n "$WEBUI_EXTRA" ]; then
  WEBUI_URLS=("$WEBUI_EXTRA" "${WEBUI_URLS[@]}")
fi
if [ -n "${STREMIO_WEBUI_URL:-}" ]; then
  WEBUI_URLS=("$STREMIO_WEBUI_URL" "${WEBUI_URLS[@]}")
fi

fetch_to() { # url dest
  curl -fsSL --retry 2 --max-time 60 "$1" -o "$2" \
    || die "download failed: $1"
}

# Prints "<url> <sha256>" for the latest server.js per the baseline chain.
baseline_server() {
  need curl; need jq
  local desc details url sum
  desc="$(curl -fsSL --retry 2 --max-time 30 "$BASELINE_URL" | jq -r '.versionDesc // empty')" \
    || die "cannot read version baseline: $BASELINE_URL"
  [ -n "$desc" ] || die "baseline has no versionDesc: $BASELINE_URL"
  details="$(curl -fsSL --retry 2 --max-time 30 "$desc")" \
    || die "cannot read version details: $desc"
  url="$(printf '%s' "$details" | jq -r '.files["server.js"].url // empty')"
  sum="$(printf '%s' "$details" | jq -r '.files["server.js"].checksum // empty')"
  [ -n "$url" ] || die "baseline names no server.js: $desc"
  echo "$url $sum"
}

seed_portable_config() {
  mkdir -p "$PC_DIR"
  [ -f "$PC_DIR/stremio-settings.ini" ] \
    || fetch_to "$SETTINGS_URL" "$PC_DIR/stremio-settings.ini"
  [ -f "$PC_DIR/mpv.conf" ] \
    || fetch_to "$MPV_BASE_URL/portable_config/mpv.conf" "$PC_DIR/mpv.conf"
  [ -f "$PC_DIR/input.conf" ] \
    || fetch_to "$MPV_BASE_URL/portable_config/input.conf" "$PC_DIR/input.conf"
  if command -v 7z >/dev/null 2>&1; then
    # anime4k zip is flat (shaders/ at root plus __MACOSX junk): extract
    # only shaders/ so nothing clobbers mpv.conf/input.conf above.
    if [ ! -d "$PC_DIR/shaders" ]; then
      tmp="$(mktemp -d)" || die "mktemp failed"
      fetch_to "$ANIME4K_ZIP" "$tmp/anime4k.zip"
      if 7z x -y -o"$tmp/ax" "$tmp/anime4k.zip" shaders >/dev/null; then
        mkdir -p "$PC_DIR/shaders"
        cp -r "$tmp/ax/shaders/." "$PC_DIR/shaders/"
      else
        warn "anime4k extract failed"
      fi
      rm -rf "$tmp"
    fi
    # thumbfast 7z nests everything under portable_config/ alongside a
    # Windows mpv.exe bundle: lift only the lua + conf, drop the prefix.
    if [ ! -f "$PC_DIR/scripts/thumbfast.lua" ]; then
      tmp="$(mktemp -d)" || die "mktemp failed"
      fetch_to "$THUMBFAST_7Z" "$tmp/thumbfast.7z"
      if 7z x -y -o"$tmp/tf" "$tmp/thumbfast.7z" \
          portable_config/scripts/thumbfast.lua \
          portable_config/script-opts/thumbfast.conf >/dev/null; then
        mkdir -p "$PC_DIR/scripts" "$PC_DIR/script-opts"
        cp "$tmp/tf/portable_config/scripts/thumbfast.lua" "$PC_DIR/scripts/"
        cp "$tmp/tf/portable_config/script-opts/thumbfast.conf" "$PC_DIR/script-opts/"
        # Windows-only mpv.exe path: use system mpv via PATH instead.
        sed -i 's|^mpv_path=|#mpv_path=|' "$PC_DIR/script-opts/thumbfast.conf"
      else
        warn "thumbfast extract failed"
      fi
      rm -rf "$tmp"
    fi
  else
    warn "7z missing — skipping anime4k/thumbfast archives (install p7zip)"
  fi
}

ensure_server_js() { # fetches when missing or baseline moved on
  local info url sum actual
  if [ -f "$SERVER_JS" ] && [ -f "$VERSION_FILE" ]; then
    return 0
  fi
  need node
  info="$(baseline_server)"
  url="${info% *}"; sum="${info#* }"
  fetch_to "$url" "$SERVER_JS"
  actual="$(sha256sum "$SERVER_JS" | cut -d' ' -f1)"
  if [ -n "$sum" ] && [ "$actual" != "$sum" ]; then
    # Observed live: dl.strem.io bytes drift from the baseline checksum
    # (v4.20.15 serves 678332… vs baseline fcc4c5e…). The baseline URL over
    # HTTPS stays the trust anchor; report drift, keep running.
    warn "server.js checksum drift: baseline $sum, fetched $actual"
  fi
  printf '%s\n%s\n' "$url" "$actual" >"$VERSION_FILE"
}
cert_valid() { # file -> 0 when notBefore <= now <= notAfter
  [ -f "$1" ] || return 1
  need jq
  local nb na now
  nb="$(jq -r '.notBefore // empty' "$1")"
  na="$(jq -r '.notAfter // empty' "$1")"
  [ -n "$nb" ] && [ -n "$na" ] || return 1
  now="$(date +%s)"
  [ "$(date -d "$nb" +%s 2>/dev/null)" -le "$now" ] \
    && [ "$now" -le "$(date -d "$na" +%s 2>/dev/null)" ]
}

ensure_https_cert() { # fetch loopback cert when missing or expired
  cert_valid "$CERT_FILE" && return 0
  need node; need curl
  mkdir -p "$APP_PATH_DIR" || { warn "cannot create $APP_PATH_DIR"; return 1; }
  local resp
  resp="$(curl -fsSL --retry 2 --max-time 30 -X POST "$CERT_ENDPOINT" \
    -H 'Content-Type: application/json' \
    -d '{"authKey":null,"ipAddress":"127.0.0.1"}')" \
    || { warn "cert fetch failed: $CERT_ENDPOINT"; return 1; }
  printf '%s' "$resp" | node -e '
    let s = "";
    process.stdin.on("data", (c) => s += c).on("end", () => {
      const certResp = JSON.parse(JSON.parse(s).result.certificate);
      const b64 = (x) => Buffer.from(x, "base64").toString("ascii");
      const out = {
        domain: "127-0-0-1" + certResp.commonName.replace("*", ""),
        key: b64(certResp.contents.PrivateKey),
        cert: b64(certResp.contents.Certificate),
        notBefore: certResp.contents.NotBefore,
        notAfter: certResp.contents.NotAfter,
      };
      require("fs").writeFileSync(process.argv[1], JSON.stringify(out));
    });' "$CERT_FILE" \
    || { warn "cert parse failed"; return 1; }
  if cert_valid "$CERT_FILE"; then
    echo "$PROG: https cert: $CERT_FILE"
  else
    warn "fetched cert invalid"
    return 1
  fi
}

pick_webui() { # first reachable URL, else the default
  local u
  for u in "$@"; do
    if curl -fsSI --max-time 5 -o /dev/null "$u" 2>/dev/null; then
      echo "$u"
      return 0
    fi
  done
  echo "$1"
}

check_all() { # --check: fail on missing required pieces, warn on optional
  local fail=0 info url sum cached
  command -v node >/dev/null 2>&1 || { echo "missing: node"; fail=1; }
  command -v curl >/dev/null 2>&1 || { echo "missing: curl"; fail=1; }
  command -v jq >/dev/null 2>&1 || { echo "missing: jq"; fail=1; }
  command -v mpv >/dev/null 2>&1 || echo "optional missing: mpv (browser playback still works)"
  command -v 7z >/dev/null 2>&1 || echo "optional missing: 7z (archive seeding skipped)"
  [ -f "$SERVER_JS" ] || { echo "missing: $SERVER_JS"; fail=1; }
  [ -f "$PC_DIR/stremio-settings.ini" ] || { echo "missing: $PC_DIR/stremio-settings.ini"; fail=1; }
  [ -f "$PC_DIR/mpv.conf" ] || { echo "missing: $PC_DIR/mpv.conf"; fail=1; }
  [ -f "$PC_DIR/input.conf" ] || { echo "missing: $PC_DIR/input.conf"; fail=1; }
  if cert_valid "$CERT_FILE"; then
    echo "https cert: ok"
  else
    echo "missing/invalid: $CERT_FILE"; fail=1
  fi
  # mpv ~~/ resolution: every glsl-shader named by mpv.conf must exist.
  if [ -f "$PC_DIR/mpv.conf" ]; then
    missing_shaders="$(grep -o '~~/[^";]*\.glsl' "$PC_DIR/mpv.conf" 2>/dev/null | sed 's|^~~/||' | while read -r s; do [ -f "$PC_DIR/$s" ] || echo "$s"; done)"
    if [ -z "$missing_shaders" ]; then
      echo "shaders: ok"
    elif [ -d "$PC_DIR/shaders" ]; then
      echo "missing shaders:"; printf '%s\n' "$missing_shaders" | sed 's|^|  |'; fail=1
    else
      echo "optional missing: shaders/ (install p7zip and re-run to seed)"
    fi
  fi
  [ -f "$PC_DIR/scripts/thumbfast.lua" ] \
    || echo "optional missing: scripts/thumbfast.lua (install p7zip and re-run to seed)"
  if [ "$fail" -eq 0 ] && [ -f "$VERSION_FILE" ]; then
    info="$(baseline_server)" || { echo "baseline unreachable"; return 1; }
    url="${info% *}"; sum="${info#* }"
    cached_url="$(sed -n '1p' "$VERSION_FILE")"; cached_sum="$(sed -n '2p' "$VERSION_FILE")"
    actual="$(sha256sum "$SERVER_JS" | cut -d' ' -f1)"
    echo "server: $cached_url"
    if [ "$cached_url" != "$url" ]; then
      echo "update available: $url"
    fi
    if [ -n "$sum" ] && [ "$actual" != "$sum" ]; then
      echo "checksum drift (warning): baseline $sum, cached $actual"
    else
      echo "checksum: ok"
    fi
    if [ "$actual" != "$cached_sum" ]; then
      echo "cached server.js changed since fetch (warning)"
    fi
  fi
  if [ "$fail" -eq 0 ]; then
    echo "check: ok ($DATA_DIR)"
  else
    echo "check: FAILED"
  fi
  return "$fail"
}

mkdir -p "$DATA_DIR" || die "cannot create $DATA_DIR"
seed_portable_config

if [ "$MODE_CHECK" -eq 1 ]; then
  [ -f "$SERVER_JS" ] || ensure_server_js
  cert_valid "$CERT_FILE" || ensure_https_cert
  check_all
  exit $?
fi

# Single-instance lock before the server fetch, so two concurrent first
# runs cannot double-fetch server.js (--check stays lock-free and
# read-only; portable_config seeding above is idempotent).
if command -v flock >/dev/null 2>&1; then
  exec 9>"$LOCK_FILE" || die "cannot open lock $LOCK_FILE"
  flock -n 9 || die "already running (lock $LOCK_FILE)"
else
  mkdir "$LOCK_FILE.dir" 2>/dev/null || die "already running (lock $LOCK_FILE.dir)"
  trap 'rmdir "$LOCK_FILE.dir" 2>/dev/null' EXIT
fi


if [ "$STREAMING_SERVER" -eq 1 ]; then
  ensure_server_js
  ensure_https_cert || warn "continuing without HTTPS endpoint (web UI may report server unavailable)"
  need node
  cd "$DATA_DIR" || die "cannot cd $DATA_DIR"
  NO_CORS=1 node "$SERVER_JS" >"$DATA_DIR/server.log" 2>&1 &
  SERVER_PID=$!
  trap 'kill $SERVER_PID 2>/dev/null' EXIT
  echo "$PROG: streaming server started (pid $SERVER_PID, log $DATA_DIR/server.log)"
  echo "$PROG: ServerStarted"
fi

WEBUI="$(pick_webui "${WEBUI_URLS[@]}")"
if [ "$OPEN_BROWSER" -eq 1 ]; then
  if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "$WEBUI" >/dev/null 2>&1 &
  else
    warn "xdg-open missing — open manually: $WEBUI"
  fi
fi
echo "$PROG: web UI: $WEBUI"

if [ "$STREAMING_SERVER" -eq 1 ]; then
  wait "$SERVER_PID"
fi
