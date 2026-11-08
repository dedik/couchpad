#!/usr/bin/env bash
# Generuje mapy bitowe kroju Ubuntu dla LVGL.
#
# LVGL nie czyta plików TTF w czasie działania: lv_font_conv zamienia krój na mapy
# bitowe osobno dla każdego rozmiaru. Podmiana kroju to ponowne uruchomienie tego
# skryptu, bez zmian w kodzie ekranów.
#
# Projekt używa wyłącznie odmian 400, 500 i 700 — tych, które Ubuntu ma naprawdę,
# więc nic nie jest pogrubiane sztucznie.
#
#   ./tools/gen-fonts.sh [katalog-z-ttf]
#
# Licencja Ubuntu Font Licence pozwala osadzić krój w urządzeniu.
set -euo pipefail

src="${1:-$HOME/Library/Fonts}"
out="$(cd "$(dirname "$0")/.." && pwd)/components/ui/assets/fonts"
mkdir -p "$out"

# Łacina podstawowa plus polskie znaki i kilka znaków interpunkcyjnych,
# których używa interfejs: stopień, kropka środkowa, mnożenie, półpauza, wielokropek.
# 0x14D i 0x160/0x161 są na tytuły pozycji z biblioteki (np. „Shōgun").
latin="0x20-0x7E,0xB0,0xB7,0xD3,0xD7,0xF3"
polish="0x104,0x105,0x106,0x107,0x118,0x119,0x141,0x142,0x143,0x144,0x15A,0x15B,0x179,0x17A,0x17B,0x17C"
extra="0x14D,0x160,0x161,0x2013,0x2014,0x2026"
charset="$latin,$polish,$extra"

gen() {
  local name="$1" ttf="$2" size="$3" range="$4"
  echo "  $name  ($size px)"
  npx --yes lv_font_conv@1.5.3 \
    --font "$src/$ttf" \
    --range "$range" \
    --size "$size" \
    --bpp 4 \
    --no-compress \
    --format lvgl \
    --lv-include lvgl.h \
    --force-fast-kern-format \
    -o "$out/$name.c"
}

echo "Krój z: $src"
gen ubuntu_r_11 Ubuntu-Regular.ttf 11 "$charset"
gen ubuntu_r_13 Ubuntu-Regular.ttf 13 "$charset"
gen ubuntu_r_15 Ubuntu-Regular.ttf 15 "$charset"
gen ubuntu_m_11 Ubuntu-Medium.ttf  11 "$charset"
gen ubuntu_m_13 Ubuntu-Medium.ttf  13 "$charset"
gen ubuntu_m_15 Ubuntu-Medium.ttf  15 "$charset"
gen ubuntu_b_15 Ubuntu-Bold.ttf    15 "$charset"
gen ubuntu_b_19 Ubuntu-Bold.ttf    19 "$charset"
gen ubuntu_b_26 Ubuntu-Bold.ttf    26 "$charset"
# Duże odczyty liczbowe: same cyfry, dwukropek zegara, przecinek dziesiętny i pauza w miejscu
# wartości bez świeżego stanu — kilka kilobajtów zamiast kilkudziesięciu.
# 46 px: głośność i temperatura na kafelku klimy, 64 px: tarcza temperatury i zegar startu.
num="0x2C,0x30-0x3A,0x2014"
gen ubuntu_b_46_num Ubuntu-Bold.ttf 46 "$num"
gen ubuntu_b_64_num Ubuntu-Bold.ttf 64 "$num"
# 96 px: zegar ekranu startowego w poziomie (sama godzina: cyfry, dwukropek, pauza).
gen ubuntu_b_96_num Ubuntu-Bold.ttf 96 "0x30-0x3A,0x2014"

# Ikony trybów klimy i efektów lampy nie są już czcionką Lucide: to obrazy pixel art
# z tools/draw_option_icons.py, przerabiane przez tools/gen_assets.py (2026-11-08).

echo "Gotowe: $out"
