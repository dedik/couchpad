# Plan: the WIĘCEJ (settings) screen and refactoring before further features

[Polski](2026-10-31-ustawienia-i-refaktor.md)

_Draft of 2026-10-31 for approval. None of this has been done yet._

## Context

- **The "WIĘCEJ" (MORE) screen has seven rows, but only three work:** Wi-Fi, MQTT and Simulation.
  - Brightness, Sleep, Diagnostics and About device are dummies with no action (`screen_more.cpp:74-88`).
  - Brightness (100 %) and sleep (30 s) are hard-coded.
  - The version "v0.1" is a plain string.
- **Ahead of us: LG TV via HA, battery, OTA and the Emby library.** A code review revealed several places worth fixing first, because each of these features would either duplicate them or trip over them.

The plan has two parts:
- **A:** the target settings screen.
- **B:** refactoring, arranged so that A and the TV can already be built on the new foundation.

---

## Part A — the WIĘCEJ screen

### Layout

A list in sections, each row opens a subpage (`widgets::make_page`), as Wi-Fi and MQTT do today. The rule from the mockup stays: a sixth screen does not get a tab, only a row here, and the touch target is at least 68 px.

```text
CONNECTIONS
  Wi-Fi                     network name · signal strength     (exists)
  Home Assistant and MQTT   broker, Z906, remote identifier    (exists, cleanup)
SCREEN
  Brightness                slider 10–100 %                    NEW
  Screen timeout            15 s / 30 s / 1 min / 2 min / never NEW
  Night mode                dimmer during chosen hours         NEW
  Start screen              TV / Apple TV / Speakers / Home / last NEW
CONTROLS
  Keyboard                  Apple TV / QWERTY · live text      (exists in the editor, added here)
  Apple TV trackpad         sensitivity · large / small layout NEW
ROOM AND TIME
  Room name                 "SALON" in the header              NEW
  Time zone                 list of zones                      NEW
SYSTEM
  Diagnostics               link status, signal, memory, uptime NEW
  About device              version, build date, MAC, board    NEW
  Test mode                 fake data + simulation panel       (moved from two places)
  Restart                                                      NEW
  Restore factory settings                                     NEW
```

### Setting details

| Setting | Behaviour | Technical notes |
| --- | --- | --- |
| **Brightness** | Slider 10–100 % in steps of 10, instant preview, saved when the finger is lifted. | **Pitfall:** `bsp_display_backlight_on()` always sets 100 %, so after every wake-up the brightness would return to 100 %. `board::set_display_on(true)` must restore the saved value. Lower limit 10 %, so that the screen cannot be "turned off" permanently. |
| **Screen timeout** | 15 s, 30 s, 1 min, 2 min, never. | Today `kIdleTimeoutMs` in `ui.cpp:11`. "Never" only when powered from USB — once a battery is added, the option disappears or shows a warning. |
| **Night mode** | From–to (e.g. 22:00–6:00) brightness X %. | The board has no light sensor, so the "automatic after dark" from the mockup can only work by the time from SNTP. Without a known time, night mode is disabled. |
| **Start screen** | Tab shown after boot and after wake-up; "last" remembers where you left off. | Today always TV (`ui.cpp:116`). |
| **Keyboard** | Apple TV / QWERTY style and live text. | The same NVS as today's chips in the editor (`kb_qwerty`, `kb_live`). The chips stay; this is a second way in. |
| **Trackpad** | Sensitivity: low / medium / high (step 34 / 26 / 20 px). Layout: large trackpad without the library / with the library. | Today `kTrackpadStep` and the `kCompactLayout` constant in the code. |
| **Room name** | Text in the tab header ("SALON" (LIVING ROOM)). | Today hard-coded in `shell.cpp:23-27`. |
| **Time zone** | A few zones (Poland by default, UTC, London, …). | Today `kTimezone` in `time_sync.cpp:15`. Fixed list in the firmware, without typing a POSIX string. |
| **Diagnostics** | Wi-Fi status (signal in dBm, refreshed every few seconds), IP, Z906 and HA separately (connected / error / last state from X s ago), snapshot revision, last command, free memory, uptime, time since clock synchronisation. | RSSI is read today only once when connecting; `esp_wifi_sta_get_ap_info` has to be polled, but only while the page is open. |
| **About device** | Version and build date from `esp_app_get_description()`, board, MAC, ESP32-C6 firmware version. | Version from `PROJECT_VER` in `CMakeLists.txt` instead of the "v0.1" string. |
| **Test mode** | Fake-data switch and simulation panel in one place. | Today the switch is on the MQTT page and the panel in a separate row. |
| **Restart** | Confirmation, then `esp_restart()`. | — |
| **Restore factory settings** | Two-step confirmation, erases NVS (Wi-Fi, MQTT, settings), restart. | Safest via a new `storage::factory_reset()` function rather than erasing keys by hand. |

### Deliberately postponed

Each of these requires hardware or an ADR:

- **Battery and power saving** (level, profile): the MAX17048 exists only in the enclosure plans, and radio sleep extends wake-up to ~8 s (ADR 0005).
- **OTA update:** the partition table has no OTA slots, so a new partition table, a one-time flash over cable and an ADR are needed. This is a strong candidate for the next big stage: tvOS changes protocols, and a remote without OTA has to be flashed over USB.
- **Choosing apps for the tiles from the remote:** requires the bridge to publish the full list of apps, i.e. a contract change.
- **Language and light theme:** no need, and the cost is fonts and duplicated styles.

---

## Part B — code review and refactoring

### Defects found along the way (fix first)

1. **The Z906 breaks the "touch does not wait for the network" rule.**
   - `Z906Client::publish_value` calls the blocking `esp_mqtt_client_publish` from the UI task (`z906_client.cpp:243`).
   - With weak Wi-Fi, the volume dial can freeze the screen.
   - The HA client already uses the non-blocking `enqueue`.
2. **NVS read on every render.**
   - `render_more` and `render_mqtt` call `storage::simulation_enabled()`, i.e. opening, reading and closing NVS.
   - `render()` redraws all screens after every snapshot.
3. **Every snapshot clears all pending states.**
   - The volume echo from the Z906 clears "pending" on the Apple TV tile before HA confirms (`app_controller.cpp:80`).
   - With the TV this will be even more visible.
4. **TV, scene and shortcut commands in real mode go nowhere.**
   - `LiveSource` sends them to the Z906 client, which silently ignores them.
   - The TV then shows "pending", and after 3 s "no confirmation".
5. **Minor:**
   - `static char client_id` in `Z906Client::start` is shared between instances.
   - `Z906Client::error_` is written from the MQTT task and read from the UI without atomicity.
   - The HA client does not reset `last_revision_` after reconnecting, so a counter reset in HA would freeze the Apple TV until the remote restarts.
   - Leaving test mode does not stop the simulation task.
   - `storage::init()` is called twice.

### Refactorings by value

| # | Change | Why | Risk |
| --- | --- | --- | --- |
| 1 | **A single MQTT session** (`transport::MqttSession`): connection, topic router, fragment reassembly, non-blocking send, errors. Z906 and HA become thin codecs. | ~150 fewer duplicated lines. Fixes defects 1 and 5. The LG TV becomes ~50 lines of codec instead of a third client and a third TCP connection to the broker. | medium |
| 2 | **Per-device events instead of copies of the whole snapshot** (~2 kB). Pending states cleared per section. | No more 2 kB copies on the 8–12 kB LVGL stack (this has already caused a restart loop once) and no more Z906 queue taking ~24 kB. Fixes defect 3. A precondition for adding the TV. | medium |
| 3 | **`Device` in the intent and a route table in `LiveSource`** instead of string comparisons `target == "appletv"`. | Adding the TV is one table row; unhandled intents are explicit (defect 4). | low |
| 4 | **The v1 contract codec as pure functions** (`parse_state`, `encode_command`, the Z906 topic codec) **+ tests on the computer**. | Today there are zero tests. These are the most frequently changed places (TV, Emby). Tests: AppState, AppController, codecs, keyboard layouts. | low |
| 5 | **A settings service in `application`** with a cache and an NVS helper (RAII, `get`/`set` with a default value, schema version). | Foundation for part A. Fixes defect 2. Removes `storage` calls from `widgets.cpp` and the screens. | low |
| 6 | **Splitting `widgets.cpp` (1821 lines)**: controls, dial and level, navigation (D-pad, trackpad, gestures), editor with keyboard. One `bind<T>()` helper instead of eight variants of callback binding. `make_sheet()` for overlays. | Part A will add more widgets (brightness slider, pick from a list, confirmation); they are easier to add to smaller files. | low, mechanical |
| 7 | **Removing the dead library** "Nowe filmy / Nowe seriale" (New movies / New series) (half of `screen_appletv.cpp`, `AtvPlay`, 12 posters, ~2k lines). Simulation behind a Kconfig option. | We will design Emby from scratch from the contract anyway. Less flash and fewer globals. | low |
| 8 | **UI without dependencies on `transport` and `storage`**: a view model from `application` (link status, signal, errors, clock), source switching in `application`, a device table instead of `switch (ScreenId)` in the header. | A single place to hook up the battery, OTA and power saving. | medium |

In addition: timing constants in a single `core/config.hpp` (some in Kconfig) and fixing outdated comments that still describe simulation as the main source.

---

## Proposed order

Each stage ends with a build, flashing with approval and a check on the remote, followed by a separate commit.

1. **Stage 0 — quick fixes** (defects 1, 2, 5, no architecture changes). Result: the volume dial does not block the UI, and rendering stops reading NVS.
2. **Stage 1 — cleanup:**
   - refactoring 7 (dead code);
   - refactoring 6 (splitting the widgets);
   - refactoring 5 (settings service);
   - constants in `core/config.hpp`.

   Behaviour does not change.
3. **Stage 2 — the WIĘCEJ screen, first batch:**
   - brightness (with the fix for the backlight pitfall), screen timeout, start screen;
   - About device with the real version, restart, restore factory settings;
   - test mode in one place.
4. **Stage 3 — the WIĘCEJ screen, second batch:**
   - diagnostics;
   - keyboard and trackpad;
   - room name, time zone, night mode.
5. **Stage 4 — foundation for the TV:** refactorings 1, 2 and 3, then the tests from refactoring 4. After it, the LG TV is mainly a codec and the bridge.
6. **Stage 5 (separate ADR) — OTA.** Then the battery, once the hardware is available.

Refactoring 8 can be spread out: at stage 3 move the link status into the view model, and the rest at stage 4.

## Verification

- `idf.py build` with no new warnings.
- After stage 4, a test target on the computer (`ctest`) for the domain and the codecs.
- On the remote:
  - every setting survives a restart;
  - brightness comes back after wake-up;
  - "restore factory settings" leads to the Wi-Fi setup screen;
  - the volume dial does not stutter with a weak signal;
  - boot log without restarts and without stack overflow.

## Questions for decision

1. Do you accept the list of settings from part A? What is missing, and what is unnecessary?
2. Do we remove the "Nowe filmy / Nowe seriale" library for good (refactoring 7)? After Emby we will design it from scratch.
3. Order: the settings screen first (stages 0–3), or the TV foundation first (stage 4)?
4. Should OTA be the next big stage after the settings?
