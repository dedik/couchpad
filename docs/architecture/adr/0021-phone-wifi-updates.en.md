# ADR 0021 — Updating the Android app over Wi-Fi

[Polski](0021-phone-wifi-updates.md)

- **Date:** 2026-11-10
- **Status:** accepted
- **Extends:** [ADR 0016](0016-android-app.en.md)

## Problem

Phones running the remote stay at home as dedicated remotes. Every new version meant
connecting the phone to a laptop with a cable and running `adb install`. Wireless adb debugging
does not fix this for good: Xiaomi turns it off after a reboot or a network change, and a laptop
is still needed.

## Decision

- **The app offers the update itself.** The laptop publishes a new version, and every phone
  running the remote learns about it from the broker it is already connected to.
- **The manifest** is a retained message on `multimedia_remote/app/android` (QoS 1), shared by
  all phones:
  ```json
  {"version_code": 57, "version_name": "0.3.57",
   "url": "http://<ha>:8123/local/pilot/pilot-57.apk",
   "sha256": "<64 hex chars>", "size": 14680064}
  ```
  The app subscribes to the topic after every broker connection (`PilotRepository.release`) and
  shows the version only when `version_code` is higher than the installed one. An incomplete
  manifest is ignored.
- **The APK lives in Home Assistant**, in the config's `www/pilot` directory (URL
  `/local/pilot/`), without login, on the home network only. The file name contains the version
  number, so no cache serves a stale file; the script keeps the last two versions.
- **Installation** (`data/AppUpdater.kt`): download to `cacheDir`, sha256 check, a
  `PackageInstaller` session. On Android 12+ the session asks for `USER_ACTION_NOT_REQUIRED`
  (permission `UPDATE_PACKAGES_WITHOUT_USER_ACTION`): the system installs without a dialog if the
  remote is its own installer of record. That holds after the first Wi-Fi update; a cable
  install with plain `adb install` resets it (use `adb shell pm install -r -i
  app.couchpad.pilot …`). Older systems and the first update show Android's dialog. On the
  first update Android 8+ requires a
  one-time "Install unknown apps" permission for the remote; the Phone page leads to that screen
  and re-checks the permission on return.
- **UI:** *Settings → Phone → App version*. Without a newer version: "aktualna" (up to date).
  With one: "Install version …", then download progress, "Installing…" or an error with
  "Try again".
- **The version number** is the repository commit count (`git rev-list --count HEAD` in
  `app/build.gradle`), `versionName` = `0.3.<count>`. It grows with every commit without manual
  bumps.
- **Publishing:** `phone/tools/publish-update.sh` builds the APK, uploads it over SSH to
  `www/pilot`, checks it is reachable at its URL and publishes the manifest via HA's
  `mqtt.publish`. It refuses to publish with uncommitted changes (`--dirty` forces it), so the
  version number always points at a commit. Addresses, the SSH account and the command that
  prints the HA token live in `phone/tools/update.env`, outside git (template:
  `update.env.example`).

## Consequences

- All versions must be signed with the same key. Today that is the laptop's debug key
  (`~/.android/debug.keystore`); losing it means a one-time cable reinstall on every phone
  (losing the remote's settings).
- The first update needs the permission and confirmations: one on Samsung (One UI), several
  MIUI screens on Xiaomi (HyperOS/MIUI). Later ones on Android 12+ are a single
  "Install version …" tap in the remote. Verified on both on 2026-11-10.
- Android's dialog can be skipped, the system scanners cannot: every version has a new
  checksum, so Google Play Protect ("App scan recommended") or the vendor's scanner (MIUI) may
  stop an update and ask for a decision. The app does not change these settings.
- The remote shows over the lock screen (ADR 0010), but Android's permission and installer
  dialogs do not — the phone has to be unlocked.
- After installation Android kills the remote's process; as the home screen it comes back on
  Home.
- The APK under `/local` is reachable by anyone on the home network without login. It holds
  no secrets — the broker settings live on the phone, not in the APK.
