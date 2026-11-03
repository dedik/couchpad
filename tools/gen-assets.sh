#!/usr/bin/env bash
# Przerabia logotypy, plakaty i ikony zakładek z makiety Pulpitu na obrazy LVGL.
# Potrzebuje Inkscape'a (rasteryzacja SVG) oraz Pillow w lokalnym środowisku Pythona.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
venv="$here/.venv"

if [ ! -x "$venv/bin/python" ]; then
  echo "Tworzę środowisko Pythona w $venv"
  python3 -m venv "$venv"
  "$venv/bin/pip" -q install pillow
fi

exec "$venv/bin/python" "$here/gen_assets.py" "$@"
