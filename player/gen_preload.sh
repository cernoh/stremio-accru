#!/usr/bin/env bash
# gen_preload.sh — embed player/preload.js as a C string header.
#
# Emits player/preload.inc defining SA_PRELOAD_JS. Run from the repository
# root before compiling player/ (the flake build does this automatically).
set -eu
DIR="$(cd "$(dirname "$0")" && pwd)"
{
  printf 'static const char SA_PRELOAD_JS[] =\n'
  sed 's/\\/\\\\/g; s/"/\\"/g; s/^/"/; s/$/\\n"/' "$DIR/preload.js"
  printf ';\n'
} > "$DIR/preload.inc"
echo "player/preload.inc written ($(wc -c < "$DIR/preload.inc") bytes)"
