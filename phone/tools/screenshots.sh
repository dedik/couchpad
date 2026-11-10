#!/bin/sh
# Zrzuty wszystkich ekranów natywnej aplikacji z podłączonego telefonu (adb).
#
#   tools/screenshots.sh portrait|landscape [light]
#
# Współrzędne są dla Galaxy M23 (1080 × 2408). Skrypt na chwilę wymusza orientację
# (i z `light` styl jasny) w ustawieniach aplikacji, a na końcu przywraca poprzednie.
# Zrzuty w stylu jasnym trafiają do docs/screenshots/<orientacja>/light/. Nie wysyła żadnych poleceń do
# urządzeń: tylko przechodzi między ekranami i otwiera listy wyboru.
set -eu

MODE="${1:-portrait}"
STYLE="${2:-dark}"
APP=app.couchpad.pilot
OUT="$(cd "$(dirname "$0")/.." && pwd)/docs/screenshots/$MODE"
[ "$STYLE" = light ] && OUT="$OUT/light"
mkdir -p "$OUT"
# Stare zrzuty usuwamy: po dodaniu ekranu zmienia się numeracja plików.
rm -f "$OUT"/*.png

tap() { adb shell input tap "$1" "$2"; sleep "${3:-2}"; }
back() { adb shell input keyevent KEYCODE_BACK; sleep 1.5; }
shot() { adb exec-out screencap -p > "$OUT/$1.png"; echo "  $1"; }
# Ustawienie typu int w shared_prefs aplikacji: get <nazwa>, set <nazwa> <wartość>.
get_pref() {
    adb shell "run-as $APP cat shared_prefs/pilot.xml" | sed -n "s/.*name=\"$1\" value=\"\([0-9]*\)\".*/\1/p"
}
set_pref() {
    adb shell am force-stop "$APP"
    adb shell "run-as $APP sed -i 's/name=\"$1\" value=\"[0-9]*\"/name=\"$1\" value=\"$2\"/' shared_prefs/pilot.xml"
}

adb shell input keyevent KEYCODE_WAKEUP
OLD_ORIENTATION="$(get_pref orientation)"
OLD_THEME="$(get_pref theme)"
if [ "$MODE" = landscape ]; then set_pref orientation 2; else set_pref orientation 1; fi
# Styl: 0 ciemny, 1 jasny, 2 według godzin.
if [ "$STYLE" = light ]; then set_pref theme 1; else set_pref theme 0; fi
adb shell am start -n "$APP/.MainActivity" >/dev/null
sleep 5

echo "$MODE → $OUT"
if [ "$MODE" = landscape ]; then
    shot 01-start
    tap 1400 300 3;  shot 03-tv
    tap 90 109;      shot 02-remote
    tap 90 454;      shot 04-appletv
    tap 150 626;     shot 05-audio
    tap 90 799;      shot 06-home
    tap 482 482;     shot 07-room
    tap 700 700;     shot 08-light
    tap 2156 556 1.5; shot 09-effects; back
    tap 1095 875;    shot 10-bulb; back; back
    tap 1700 700;    shot 11-climate; back; back
    tap 90 971; shot 12-settings
else
    shot 01-start
    tap 289 795 3;   shot 03-tv
    tap 112 2324;    shot 02-remote
    tap 454 2324;    shot 04-appletv
    tap 625 2324;    shot 05-audio
    tap 796 2324;    shot 06-home
    tap 265 760;     shot 07-room
    tap 480 945;     shot 08-light   # lampa pod kartą „W pokoju” (ADR 0017)
    tap 881 1801 1.5; shot 09-effects; back
    tap 207 2066;    shot 10-bulb; back; back
    tap 482 1245;    shot 11-climate; back; back
    tap 967 2324;    shot 12-settings
    tap 540 645;     shot 13-display; back
fi

set_pref orientation "${OLD_ORIENTATION:-0}"
set_pref theme "${OLD_THEME:-0}"
adb shell am start -n "$APP/.MainActivity" >/dev/null

# Zbiorczy obraz do README (docs/screenshots/overview-*.png) z właśnie zrobionych zrzutów.
HERE="$(cd "$(dirname "$0")" && pwd)"
"$HERE/../../tools/.venv/bin/python" "$HERE/overview.py" "$MODE" "$STYLE"
echo "gotowe"
