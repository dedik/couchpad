#!/usr/bin/env bash
# Publikuje nową wersję aplikacji pilota dla telefonów w domu (ADR 0020):
#   1. buduje APK (versionCode = liczba commitów),
#   2. wgrywa go do katalogu `www/pilot` Home Assistanta (adres /local/pilot/…),
#   3. publikuje retained manifest na `multimedia_remote/app/android` przez mqtt.publish w HA.
# Telefony z pilotem pokażą wtedy w Ustawienia → Telefon „Zainstaluj wersję …”.
#
# Konfiguracja w phone/tools/update.env (poza gitem), wzór: update.env.example.
# Użycie: phone/tools/publish-update.sh [--dirty]
#   --dirty  publikuj mimo niezacommitowanych zmian (numer wersji nie wskaże wtedy commita)
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
native="$here/../native"
env_file="$here/update.env"
[[ -f "$env_file" ]] || { echo "Brak $env_file — skopiuj update.env.example i uzupełnij." >&2; exit 1; }
# shellcheck source=/dev/null
source "$env_file"
: "${UPDATE_SSH:?}" "${UPDATE_WWW_DIR:?}" "${UPDATE_BASE_URL:?}" "${HA_URL:?}" "${HA_TOKEN_CMD:?}"

if [[ "${1:-}" != "--dirty" ]] && [[ -n "$(git -C "$native" status --porcelain)" ]]; then
    echo "Niezacommitowane zmiany — zacommituj albo użyj --dirty." >&2
    exit 1
fi

if [[ -z "${JAVA_HOME:-}" && -d /opt/homebrew/opt/openjdk@17 ]]; then
    export JAVA_HOME=/opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk/Contents/Home
fi
(cd "$native" && ./gradlew -q assembleDebug)

apk="$native/app/build/outputs/apk/debug/app-debug.apk"
meta="$native/app/build/outputs/apk/debug/output-metadata.json"
code=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["elements"][0]["versionCode"])' "$meta")
name=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["elements"][0]["versionName"])' "$meta")
sha=$(shasum -a 256 "$apk" | cut -d' ' -f1)
size=$(wc -c < "$apk" | tr -d ' ')
file="pilot-$code.apk"

ssh_opts=(-o IdentitiesOnly=yes)
[[ -n "${UPDATE_SSH_KEY:-}" ]] && ssh_opts+=(-i "$UPDATE_SSH_KEY")

echo "Wgrywam $file ($name, $((size / 1024 / 1024)) MB)…"
# Nazwa z numerem wersji: żaden cache nie poda starego pliku. Zostają dwie ostatnie wersje.
ssh "${ssh_opts[@]}" "$UPDATE_SSH" "mkdir -p '$UPDATE_WWW_DIR' && cat > '$UPDATE_WWW_DIR/$file.part' && mv '$UPDATE_WWW_DIR/$file.part' '$UPDATE_WWW_DIR/$file' && cd '$UPDATE_WWW_DIR' && ls -t pilot-*.apk | tail -n +3 | xargs -r rm -f" < "$apk"

url="$UPDATE_BASE_URL/$file"
status=$(curl -s -o /dev/null -w '%{http_code}' -I "$url")
[[ "$status" == 200 ]] || { echo "Plik nie jest dostępny pod $url (HTTP $status)." >&2; exit 1; }

manifest=$(python3 -c 'import json,sys;print(json.dumps({"version_code":int(sys.argv[1]),"version_name":sys.argv[2],"url":sys.argv[3],"sha256":sys.argv[4],"size":int(sys.argv[5])}))' \
    "$code" "$name" "$url" "$sha" "$size")
body=$(python3 -c 'import json,sys;print(json.dumps({"topic":"multimedia_remote/app/android","payload":sys.argv[1],"qos":1,"retain":True}))' "$manifest")

token=$(eval "$HA_TOKEN_CMD")
curl -sf -o /dev/null -X POST -H "Authorization: Bearer $token" -H "Content-Type: application/json" \
    -d "$body" "$HA_URL/api/services/mqtt/publish"
unset token

echo "Opublikowano $name (versionCode $code): $url"
