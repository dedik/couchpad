#!/bin/sh
# Zrzuty wszystkich ekranów natywnej aplikacji z podłączonego telefonu (adb).
#
#   tools/screenshots.sh portrait|landscape
#
# Współrzędne są dla Galaxy M23 (1080 × 2408). Skrypt na chwilę wymusza orientację
# w ustawieniach aplikacji i przywraca „auto” na końcu. Nie wysyła żadnych poleceń do
# urządzeń: tylko przechodzi między ekranami i otwiera listy wyboru.
set -eu

MODE="${1:-portrait}"
APP=app.couchpad.pilot
OUT="$(cd "$(dirname "$0")/.." && pwd)/docs/screenshots/$MODE"
mkdir -p "$OUT"

tap() { adb shell input tap "$1" "$2"; sleep "${3:-2}"; }
back() { adb shell input keyevent KEYCODE_BACK; sleep 1.5; }
shot() { adb exec-out screencap -p > "$OUT/$1.png"; echo "  $1"; }
set_orientation() {
    adb shell am force-stop "$APP"
    adb shell "run-as $APP sed -i 's/name=\"orientation\" value=\"[0-9]\"/name=\"orientation\" value=\"$1\"/' shared_prefs/pilot.xml"
}

adb shell input keyevent KEYCODE_WAKEUP
if [ "$MODE" = landscape ]; then set_orientation 2; else set_orientation 1; fi
adb shell am start -n "$APP/.MainActivity" >/dev/null
sleep 5

echo "$MODE → $OUT"
if [ "$MODE" = landscape ]; then
    shot 01-start
    tap 1400 300 3;  shot 02-tv
    tap 90 330;      shot 03-appletv
    tap 150 540;     shot 04-audio
    tap 90 746;      shot 05-home
    tap 482 482;     shot 06-room
    tap 700 700;     shot 07-light
    tap 1589 482 1.5; shot 08-effects; back
    tap 1095 722;    shot 09-bulb; back; back
    tap 1700 700;    shot 10-climate; back; back
    tap 90 950; shot 11-settings
else
    shot 01-start
    tap 289 795 3;   shot 02-tv
    tap 325 2324;    shot 03-appletv
    tap 540 2324;    shot 04-audio
    tap 742 2324;    shot 05-home
    tap 265 760;     shot 06-room
    tap 480 614;     shot 07-light
    tap 540 1618 1.5; shot 08-effects; back
    tap 207 1854;    shot 09-bulb; back; back
    tap 482 922;     shot 10-climate; back; back
    tap 950 2324;    shot 11-settings
    tap 540 645;     shot 12-display; back
fi

set_orientation 0
adb shell am start -n "$APP/.MainActivity" >/dev/null
echo "gotowe"
