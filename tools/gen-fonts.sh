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

# Ikony trybów klimatyzacji: pięć znaków z Lucide 0.460.0 (licencja ISC), te same co w makiecie
# docs/ux/mockup-landscape. Plik TTF jest w pakiecie npm `lucide-static@0.460.0` (font/lucide.ttf):
#   npm pack lucide-static@0.460.0 && tar xzf lucide-static-0.460.0.tgz
# sparkles, snowflake, sun, fan, droplets — kody z package/font/info.json.
lucide="${LUCIDE_TTF:-}"
if [ -n "$lucide" ]; then
  echo "  lucide_modes_24  (24 px)"
  npx --yes lv_font_conv@1.5.3 --font "$lucide" --range 0xE417,0xE169,0xE17C,0xE37D,0xE0B9 \
    --size 24 --bpp 4 --no-compress --format lvgl --lv-include lvgl.h -o "$out/lucide_modes_24.c"
  echo "  lucide_modes_36  (36 px)"
  npx --yes lv_font_conv@1.5.3 --font "$lucide" --range 0xE417,0xE169,0xE17C,0xE37D,0xE0B9 \
    --size 36 --bpp 4 --no-compress --format lvgl --lv-include lvgl.h -o "$out/lucide_modes_36.c"
  # Efekty lampy (ADR 0014): brak, przytulnie, relaks, seans TV, świeca, ciepła biel, złota biel,
  # dzienne, zimna biel, skupienie, nocne, do snu, pobudka, puls, steampunk, diwali, alarm —
  # circle-off, sofa, leaf, tv, flame, sun-dim, sun-medium, sun, snowflake, target, moon, bed,
  # sunrise, activity, cog, sparkles, siren — oraz layout-grid na kafel „Więcej”.
  # 22 px: małe kafle i wiersze listy, 36 px: duży kafel bieżącego efektu.
  effects="0xE406,0xE2C4,0xE2DE,0xE195,0xE0D7,0xE299,0xE2B1,0xE17C,0xE169,0xE184,0xE122,0xE2C1,0xE17D,0xE038,0xE30B,0xE417,0xE2EF,0xE105"
  for size in 22 36; do
    echo "  lucide_effects_$size  ($size px)"
    npx --yes lv_font_conv@1.5.3 --font "$lucide" --range "$effects" \
      --size "$size" --bpp 4 --no-compress --format lvgl --lv-include lvgl.h -o "$out/lucide_effects_$size.c"
  done
else
  echo "  lucide_modes_24 pominięte: ustaw LUCIDE_TTF=ścieżka/do/lucide.ttf"
fi

echo "Gotowe: $out"
