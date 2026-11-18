#!/bin/sh
# Odchudza telefon Samsung (One UI) do roli pilota couchpad. Bez roota, przez adb.
#
#   tools/samsung-remote.sh apply     # usuń zbędne aplikacje, przyspiesz, ustaw pilota jako ekran główny
#   tools/samsung-remote.sh restore   # przywróć usunięte aplikacje i domyślne ustawienia
#
# Sprawdzone pod Galaxy M23 (SM-M236B, Android 14). Aplikacje są usuwane tylko dla
# użytkownika 0 (`pm uninstall -k --user 0`): APK zostaje w partycji systemowej, więc
# `restore` albo przywrócenie ustawień fabrycznych oddaje wszystko. Lista jest zachowawcza:
# nie rusza niczego, od czego zależy ekran blokady, Wi-Fi, aktualizacje systemu ani konto Samsung.
set -eu

APP=app.couchpad.pilot
HOME_ACTIVITY="$APP/.MainActivity"

PACKAGES="
com.samsung.android.bixby.agent
com.samsung.android.bixby.wakeup
com.samsung.android.bixbyvision.framework
com.samsung.android.visionintelligence
com.samsung.systemui.bixby2
com.samsung.android.app.spage
com.samsung.android.game.gamehome
com.samsung.android.game.gametools
com.samsung.android.app.tips
com.samsung.android.arzone
com.samsung.android.aremoji
com.samsung.android.aremojieditor
com.samsung.android.ardrawing
com.samsung.android.stickercenter
com.samsung.android.app.reminder
com.samsung.android.app.notes
com.samsung.android.voc
com.samsung.android.forest
com.samsung.android.kidsinstaller
com.sec.android.app.kidshome
com.samsung.android.app.watchmanagerstub
com.samsung.android.mdx
com.samsung.android.samsungpass
com.samsung.android.samsungpassautofill
com.samsung.android.spay
com.samsung.android.spayfw
com.samsung.android.email.provider
com.sec.android.app.sbrowser
com.sec.android.easyMover
com.samsung.android.smartswitchassistant
com.samsung.android.da.daagent
com.samsung.android.svoiceime
com.samsung.android.app.appsedge
com.samsung.android.app.cocktailbarservice
com.samsung.android.service.peoplestripe
com.microsoft.appmanager
com.microsoft.skydrive
com.microsoft.office.officehubrow
com.linkedin.android
com.facebook.katana
com.facebook.appmanager
com.facebook.services
com.facebook.system
com.netflix.mediaclient
com.netflix.partner.activation
com.spotify.music
com.google.android.youtube
com.google.android.apps.youtube.music
com.google.android.apps.tachyon
com.google.android.apps.docs
com.google.android.apps.photos
com.google.android.videos
com.google.android.gm
"

devices=$(adb devices | sed -n '2,$p' | grep -c 'device$' || true)
if [ "$devices" != 1 ]; then
    echo "Podłącz dokładnie jeden telefon z włączonym debugowaniem USB (teraz: $devices)." >&2
    exit 1
fi

sh_adb() { adb shell "$@" </dev/null; }

case "${1:-}" in
apply)
    installed=$(sh_adb pm list packages --user 0 | sed 's/^package://' | tr -d '\r')
    echo "Usuwam zbędne aplikacje (dla użytkownika 0):"
    for p in $PACKAGES; do
        if echo "$installed" | grep -qx "$p"; then
            printf '  %-50s ' "$p"
            sh_adb pm uninstall -k --user 0 "$p" | tr -d '\r'
        fi
    done

    echo "Krótsze animacje (0.5x)."
    sh_adb settings put global window_animation_scale 0.5
    sh_adb settings put global transition_animation_scale 0.5
    sh_adb settings put global animator_duration_scale 0.5

    echo "Wygaszanie ekranu po 30 s."
    sh_adb settings put system screen_off_timeout 30000

    if echo "$installed" | grep -qx "$APP"; then
        echo "Pilot bez optymalizacji baterii i jako ekran główny."
        sh_adb dumpsys deviceidle whitelist +"$APP" >/dev/null
        sh_adb cmd appops set "$APP" RUN_ANY_IN_BACKGROUND allow
        sh_adb cmd package set-home-activity "$HOME_ACTIVITY"
    else
        echo "Brak $APP na telefonie: zainstaluj go i uruchom skrypt ponownie." >&2
    fi
    ;;
restore)
    echo "Przywracam aplikacje:"
    for p in $PACKAGES; do
        if sh_adb pm list packages -u | tr -d '\r' | grep -qx "package:$p"; then
            printf '  %-50s ' "$p"
            sh_adb cmd package install-existing --user 0 "$p" | tr -d '\r'
        fi
    done
    echo "Domyślne animacje i wygaszanie (30 s zostaje, zmień w ustawieniach)."
    sh_adb settings put global window_animation_scale 1
    sh_adb settings put global transition_animation_scale 1
    sh_adb settings put global animator_duration_scale 1
    sh_adb dumpsys deviceidle whitelist -"$APP" >/dev/null || true
    launcher=$(sh_adb cmd package query-activities --brief -a android.intent.action.MAIN \
        -c android.intent.category.HOME | tr -d '\r ' | grep '^com.sec.android.app.launcher/' | head -1)
    if [ -n "$launcher" ]; then
        echo "Ekran główny: $launcher"
        sh_adb cmd package set-home-activity "$launcher"
    fi
    ;;
*)
    sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
    ;;
esac
echo "Gotowe."
