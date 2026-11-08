# Handoff for the next person or AI

[Polski](ai-handoff.md)

## Status as of 2026-10-31

The project has two **completely independent Git repositories** in the `../` directory:

| Repository | Purpose | Status |
| --- | --- | --- |
| `../firmware` | native remote software | architecture, MQTT contract, UX specification, three mockups, board bring-up and **firmware running on hardware: five Dashboard screens, Wi-Fi via the ESP32-C6 and Z906 control directly over MQTT** |
| `../firmware/phone` | the remote on an Android phone (in this repository since 2026-11-08) | native Kotlin/Compose app in `phone/native`, SDL/LVGL port as reference; status in `phone/docs/PROGRESS.md` |
| `../enclosure` | rear enclosure for later battery use | separate scope; do not mix with firmware |

The target hardware is the Guition `JC4880P443C_I_W_Y`: the ESP32-P4 runs the application, the display and touch, and the ESP32-C6 handles connectivity. The 4.3-inch IPS 480 × 800 portrait display is to be treated as a high-quality screen — the UI should look like a finished consumer product, not an admin panel.

The Z906 already has an independent ESP32 controller and an MQTT/Home Assistant integration. The remote does not replace that controller and does not talk to it over UART or IR — it talks to it over MQTT, directly on its topics (ADR 0006).

## Priorities and boundaries

1. Interface speed comes first. The target stack is ESP-IDF + C++17 + LVGL 9, with no web layer on the device.
2. Waking up after a touch may take about 2–3 s, but after that the remote must show a **new, confirmed** state of the TV and the Z906. Old state from memory must never be presented as current.
3. Do not store tokens, passwords, private network names or infrastructure addresses in the repository.
4. Do not flash firmware, erase the flash or change wiring without the user's explicit consent.
5. Pins, timings and hardware drivers must be confirmed on the specific unit — do not copy them from similar Guition boards.

## Current copy of the factory / existing firmware

Before starting work on the UI, a full raw backup of the 16 MB flash was made. The file is deliberately ignored by Git:

```text
backups/guition-esp32p4-before-ui-2026-10-29.bin
SHA-256: 3c8e61ccdbfb659ecf0c3b6d08eab3c6a250f1ffe43a5d67ee0eaeda5dd10366
```

The backup was only read and verified with the hash; no write to the board was performed. Before any restore, the port, the flash size and the user's consent must be confirmed separately.

## Approved v1 interface

The full specification is in [`../ux/tv-and-z906-ui.md`](../ux/tv-and-z906-ui.en.md). The key decisions:

- There are two basic views: `TV` and `GŁOŚNIKI` (SPEAKERS) (Logitech Z906).
- Both have a permanently available bottom bar with these two tabs. The bar reaches the bottom edge of the display bezel, but is separated from the controls above it by a small, deliberate gap.
- The Z906 view must remain the accepted **Soundstage** layout: volume ring, sources, three vertical Center/Subwoofer/Rear sliders, effect selection and an informational `Decode 5.1`. It must not be replaced with a simplified status card.
- The TV view must contain: power, visible sources, visible picture modes, four channel shortcuts and a central D-pad with `OK`; it does not show purple labels of the current source and mode above the controls.
- At this stage the channel shortcuts are a visual element: Polsat, TVN, TVP 1 and TVP 2. Eventually each one selects the TV source and a specific channel via the bridge on the Home Assistant side.
- The TV goes directly through Glasshouse on the TV (ADR 0008, `docs/integrations/tv.md`): state and buttons over MQTT, channel shortcuts via the v1 contract through the HA bridge.

An interactive working mockup of this layout is in the repository:

```text
docs/ux/mockup/lg-tv-remote-ui.html
```

Besides the TV and Z906 screens it shows the states from ADR 0005: connecting after wake, connection error with `Ponów` (Retry), waiting for a snapshot after an action, no confirmation, and unavailable TV/Z906 (the "Symulacja stanów" (State simulation) panel below the remote). This is design material, not code to be copied to the ESP32. The actual implementation must reproduce it natively in LVGL.

## What is already confirmed

- The Z906 entity model and input ranges are described in `docs/integrations/z906.md`.
- `Decode 5.1` is information from a `binary_sensor`, never a switch.
- All Z906 sliders have a range of 0–100 and a step of 2; Center, Subwoofer and Rear are physically next to their sliders.
- The TV picture mode is a dynamic state that depends on the content; values and options must come from the current snapshot.
- After wake, ADR 0005 and `remote.sync.request` apply.

## What is still not confirmed

- Behaviour of the Wi-Fi link during longer operation: retrying after a drop, roaming, behaviour
  with a weak signal. Only connecting and automatic reconnection after a restart have been checked.
- Power consumption and power-saving modes — all the more so with the radio on.
- The TV screen on the real TV (ADR 0008, 2026-11-02): the firmware with the Glasshouse client
  compiles, but has not been flashed yet. Channels go via `tv.channel.open` through the HA bridge;
  the package with `rest_command` has not been loaded into HA yet (it requires entries in `secrets.yaml`).
- Turning the TV on from standby: Wake-on-LAN is disabled on the TV, the TV power button
  deliberately does nothing.
- The battery power approach and the rear enclosure design — that is a later stage of the `enclosure` repository.

## Bring-up completed

The hardware is confirmed: the screen lights up over the full 480 × 800, GT911 touch hits every corner,
no restarts. All values, versions and the two necessary fixes are described in
[`../hardware/verified-board-map.md`](../hardware/verified-board-map.en.md) — **start with that file**
if you are going to touch the hardware.

The most important points, so as not to lose hours on this again:

- ESP-IDF **5.5.6**, LVGL **9.2.2**, `esp_lvgl_port` **2.6.0**. A newer port does not compile with this LVGL.
- The `esp_lcd_st7701` component from the registry gives a **black screen** on this board. Only the manufacturer's patched
  copy works.
- The manufacturer's BSP must be fixed in one place: do not set the PLL clock source for the PHY in MIPI-DSI,
  because the compatibility constant crashes chips from revision 3.0 upwards. Our unit is rev v3.2.

The bring-up project was created outside the repository, as one-off material. Only confirmed results
made it into the repository.

## Interface firmware — status as of 2026-10-30

The ESP-IDF project lives in `firmware/` in the layout from `docs/architecture/README.md`: `main/` as the
composition root and the components `board`, `core`, `domain`, `application`, `ui`. Dependencies go
only downwards, `ui → application → domain`.

**The seam is real.** `application/SimulatedSource` exposes exactly the `DeviceSnapshot`
that MQTT will later provide, runs in its own FreeRTOS task and talks to the UI task through two
queues. A touch puts a command into the queue and returns; only the snapshot moves the selection.
Replacing the simulation with an MQTT client touches `main.cpp` and a new `transport` component, not the screens.

Confirmed on hardware: the five screens switch via the tab bar, the volume dial and the channel
columns change value relatively (they do not jump to the finger), the simulation panel on the `WIĘCEJ` (MORE) screen
triggers the states from ADR 0005. Boot takes 1.9 s, no restarts and no watchdog.

Tools in `tools/`: `gen-fonts.sh` generates Ubuntu bitmaps via `lv_font_conv`,
`gen-assets.sh` converts the tab icons and logos from `docs/ux/mockup-pulpit/` into LVGL
images. Both are reproducible — swapping a typeface or an icon means re-running the script.

### Connectivity — working

The ESP32-C6 responds over SDIO without any intervention in its flash: the factory firmware 2.3.2
gets along with the `esp_hosted` 3.0.10 host. Details, pins and two API pitfalls are
in [`../hardware/verified-board-map.md`](../hardware/verified-board-map.en.md).

Two components and one controller were created:

- `transport/` — `WifiService`: scanning, connecting, disconnecting in its own task, results via a queue.
- `storage/` — settings in NVS. **We store credentials only after a confirmed
  connection**, so as not to persist a wrong password. The password and the network name do not go into the log.
- `application/NetworkController` — ties one to the other, connects to the remembered
  network at boot and returns to it after a drop (an attempt every 15 s). The interface knows neither `transport` nor NVS.
  The decision to retry belongs to the controller: the Wi-Fi service only reports what happened.

**Status bar** (removed 2026-11-03, see "More room for content"). The header was split
into a status bar (24 px: time on the left, link state on the right) and an identity row (64 px). The total stays at 88 px, because the content area is
filled to the pixel — the TV screen uses 580 of 588 px and will not give up a single one. The identity
row must be exactly 64 px: that is the height of the power button, and with less LVGL clips
its rounded corners.

**Screen blanking.** After 30 s without touch the backlight turns off; the panel keeps running, so returning
is instant. Just before blanking, a transparent layer goes onto `lv_layer_sys`, so that the
first touch only wakes the device and does not press the control under the finger — otherwise waking in the dark
would turn off the TV. The time is measured by `lv_display_get_inactive_time()`, so it is reset by every
touch event, not just a click.

This is **only screen blanking**, not power saving: the chip runs normally, Wi-Fi stays up.
The mode from ADR 0004 is a separate topic and requires checking whether this board's touch reliably wakes the chosen
sleep mode.

**Waking must refresh the state of all controls.** Today the screen comes back with what was there before
blanking, and this is temporary — ADR 0005 forbids showing remembered state as
current, and during half a minute of dark screen the TV or the amplifier could have changed state
from its own remote, from an app or from an automation. Eventually, waking sends `remote.sync.request`
and keeps the overlay until a snapshot with a new revision arrives, exactly like waking from sleep.

The hook is already in place: the simulated source handles `IntentKind::RequestSync` and replays the full
sequence. Only the transport on the other side is missing, so this task will be closed together
with the MQTT client. The place to change is marked in `components/ui/ui.cpp` at `wake()`.

**We fetch the time once.** `transport/time_sync` fires at SNTP after connecting, and once the time arrives,
it stops the client. Constant polling would wake the remote when running on battery. A retry
happens only after a day, because the chip's oscillator drifts and without correction the clock drifts by minutes.
A configuration with a time server from DHCP requires a separate slot and with the default list ends with
an invalid argument error — we use a single server.

The interface gained a **second-level subpage** (`widgets::make_page`): a full-screen overlay
with a back button in the header. The tab bar has a hard five slots, so everything deeper goes this way.
The first such subpage is the Wi-Fi screen: status, remembered network, scanning, a list with signal strength
and an on-screen keyboard for the password.

### Deliberate deviations, to be closed by an ADR or a fix

- **Function icons** (arrows, power, volume) are built-in LVGL symbols from the
  Montserrat typeface, because the lucide icons from the mockup are not available locally. The tab icons are already correct — pixel art
  from `pixel-icons.js`. Eventually the whole set should go the same way as the tabs.
- **The `appletv` section entered the v1 contract** additively, via
  [ADR 0007](../architecture/adr/0007-apple-tv-via-home-assistant.en.md).
- **The Apple TV keyboard sends the text after "Zapisz" (Save)**, not live with every character.
  The shared editor does not report changes while typing; if that is a problem, it needs a
  rate-limited change callback.
- **Every snapshot clears all pending states**, so a Z906 snapshot can remove the pending
  outline from an Apple TV tile before HA confirms the launch. Visually minor, logically
  safe — the selection is moved only by the `app_id` from HA anyway.
- **Channel shortcuts, scenes and the `DOM` (HOME) screen shortcuts** are momentary commands: they have no state to
  confirm, because they are not in the contract.
- **Posters are built in** instead of downloaded over HTTP. The Emby library requires a separate ADR.

## Connectivity status

**Wi-Fi is up.** `esp_wifi_remote` + `esp_hosted` over SDIO to the ESP32-C6, pins confirmed on
this unit and described in [`../hardware/verified-board-map.md`](../hardware/verified-board-map.en.md).
The network is selected on the `WIĘCEJ` screen, the data goes to NVS.

**Audio goes directly to the Z906 controller** over MQTT, bypassing Home Assistant — the reason
and the topic table are in [ADR 0006](../architecture/adr/0006-direct-z906-topics.en.md). The client lives
in `transport/z906_client.cpp`, the adapter to `SnapshotSource` in `application/z906_source.cpp`.
The controller identifier has the form `z906_<mac>`, it is entered on the device and is stored only
in NVS — it is not in the repository and must not be written here.

**Apple TV goes through Home Assistant** ([ADR 0007](../architecture/adr/0007-apple-tv-via-home-assistant.en.md)).
The v1 contract client came back as `transport/ha_client.cpp` and reads only the `appletv` section.
`application/live_source.cpp` combines it with `Z906Source` into a single source with its own revision;
the connecting overlay is removed by the Z906, and a missing HA greys out only the Apple TV screen. The HA client starts
when the **Identyfikator pilota** (Remote ID) (`remote_id` in NVS) has been entered on the MQTT page.

Apple TV deployment status:

- the `apple_tv` integration in HA is paired; the entities, bundle ids and the entry
  identifier are in the bridge and in `domain/appletv_apps.hpp`, read from the device — see
  [`../integrations/appletv.md`](../integrations/appletv.en.md);
- on 2026-10-31 the following were checked via the HA REST API on the real Apple TV: arrows, `play_media`
  with a bundle id, sleeping and waking via `media_player.turn_off/turn_on` (**not** `remote.*`,
  which only toggles the state in HA) and `set_keyboard_text` with Polish characters;
- the hidden Siri Remote gestures are on the remote: Home ×2 (app switcher), Home hold
  (Control Centre), OK hold (context menu); checked in HA, not on the remote;
  a plain Home tap goes out ~300 ms later, because it waits for a possible second one;
- the `app_id` attribute in HA is sometimes stale, so highlighting of the active tile may lag;
- bridge package loaded into HA on 2026-10-31 (`packages/multimedia_remote.yaml`, backup
  `configuration.yaml.before-multimedia-remote.*`), `check_config` clean, after a restart the revision
  counter increases, the `appletv` section contains three apps; remote identifier `living-room-remote`;
- firmware flashed to the remote on 2026-10-31. The first version fell into a restart loop: the LVGL task
  stack overflowed, because `LiveSource::poll` held copies of the snapshot on it. Fixed
  (HA buffer as a class member, result straight into `out`) and the LVGL stack raised to 12 kB. After
  the fix the boot is clean, and the Z906 connects and receives state;
- identifier `living-room-remote` entered on the remote; Apple TV control from the remote works;
- key latency: esp-mqtt was sending the queue (`esp_mqtt_client_enqueue`) every 1000 ms —
  in HA traces the keys arrived exactly once per second, while HA executed them in 10–18 ms. Fix:
  `CONFIG_MQTT_POLL_READ_TIMEOUT_MS=20` in `sdkconfig.defaults`, navigation with QoS 0, the HA client
  retries the connection every 1.5 s like the Z906 (previously it connected 8 s after audio);
- on-screen keyboard (after commit 6ce939c, uncommitted): our own matrix instead of
  lv_keyboard, two styles — a grid like on Apple TV (default) and QWERTY like on a phone,
  a switch in the header (NVS `kb_qwerty`); the "Na żywo" (Live) mode sends text to the Apple TV every 250 ms
  while typing (NVS `kb_live`, on by default). The bridge does not publish a snapshot after
  `appletv.keyboard.*`; loaded into HA;
- Apple TV screen: 360 px trackpad, Menu and Home below it, app tiles at the bottom.
  The "Nowe filmy / Nowe seriale" (New movies / New series) library (list, posters, `AtvPlay`) removed — it will return
  redesigned from the contract, after an ADR for Emby;
- in "Dane udawane" (Fake data) mode the whole screen works: OK in YouTube slides out the keyboard, Menu hides it.

**Waking refreshes the state** in accordance with [ADR 0005](../architecture/adr/0005-fresh-state-after-wake.en.md):
a touch sends a sync request and the connecting overlay stays until a snapshot
with a new revision arrives. Only the backlight turns off — the radio stays on.

**The first state is on screen about nine seconds after boot**: two seconds for the interface,
seven for associating with Wi-Fi, the rest for the broker. The MQTT client starts before there is an IP, so the first
attempts fail — that is intended, it retries them every 1.5 s.

**The Wi-Fi signal at the remote's location is weak**: from -73 to -86 dBm, while the Z906 controller
on the same network reports -57 dBm. Because of this, associating with the network takes from five to
twelve seconds, and it is this, not the broker, that determines the time to the first state. That is exactly why the
sync timeout is counted from the last connection step.

**The TV has no source.** Its screen shows the absence of fresh state, and that is correct
until the LG integration is confirmed.

## Settings and refactoring plan

Plan: [`docs/plans/2026-10-31-ustawienia-i-refaktor.md`](../plans/2026-10-31-ustawienia-i-refaktor.en.md).

- **Stage 0 (done, checked on the remote):** the Z906 sends via `enqueue`, the trial-mode flag
  in a cache, atomic client fields, HA revision reset after connecting.
- **Stage 1 (done, compiled; not checked on the remote):**
  - the movie library removed;
  - single-byte preferences via `storage::get/set(Pref)` with a cache
    and an RAII NVS handle; a new setting is a row in `kPrefs` and in `Pref`;
    `storage::factory_reset()` is waiting for the WIĘCEJ screen;
  - `widgets.cpp` split up: basic, `widgets_controls.cpp` (dial, level),
    `widgets_nav.cpp` (D-pad, trackpad), `widgets_editor.cpp` (editor and keyboard),
    shared helpers in `widgets_internal.hpp`;
  - behaviour timings in `core/config.hpp`.
  - Not done from stage 1: the simulation behind a Kconfig option, `bind<T>()` for callbacks.
- **Stage 2, first batch (done, checked on the remote):**
  - **Display:** brightness 20–100 % remembered and restored after wake
    (`board::set_brightness`/`dim`), blanking 15 s – never, dimming 10 s before.
  - **Audio:** a limit on the Z906 volume sent from the dial.
  - **Wi-Fi:** signal in dBm, number of drops and failed attempts, last reason.
  - **About the device:** version, memory, uptime, restart and factory reset
    via `widgets::ask_confirm`.
  - The settings pages and the WIĘCEJ list scroll vertically.
  - The default values reproduce the behaviour from before the settings existed.
- **Stage 3, second batch (done, checked on the remote):**
  - **Display:** night mode (brightness at most 20 % during selected hours), screen after boot
    (including "ostatni" (last)).
  - **Room and time:** room name in the headers (NVS `room`), time zone
    (`transport::set_timezone`).
  - **Keyboard and trackpad:** style, live text, sliding out automatically or via a button,
    trackpad sensitivity (20/26/34 px).
  - The "Tekst" (Text) button on the Apple TV screen opens the keyboard without a text field on the TV.
  - **Diagnostics:** IP, link to home, Z906, HA, Apple TV, state revision.
  - Free internal memory after boot dropped to approx. 181 kB. It is worth building further pages
    on first open.
- **Next:** the foundation for the TV (stage 4 of the plan) or OTA (stage 5, a separate ADR).

## Air conditioning — status as of 2026-11-02

The `DOM` screen has an air conditioning tile (mode, target temperature, room temperature), and
tapping it opens the `Klimatyzacja` (Air conditioning) subpage: Back and power in the header, a
`widgets::Thermostat` temperature dial with −/+, operating modes, Moc / Pion / Poziom (Fan / Vertical / Horizontal) tiles
with a selection list. Design: the `docs/ux/mockup-pulpit` mockup (section "Ekran Dom i
klimatyzacja" (Home screen and air conditioning) in the README). Code: `ui/screens/screen_climate.cpp`. The Światło, Rolety,
Klimat, Kamera (Light, Blinds, Climate, Camera) shortcuts disappeared from the `DOM` screen.

- Model: `domain::ClimateState` in the snapshot — HA identifiers (`cool`, `medlow`,
  `Full`), temperatures in tenths, range and step from the entity. The Polish names are chosen by the screen.
- Controls `ClimatePower/Mode/Target/Fan/Swing/SwingH`; the temperature travels in
  `UiIntent::number`.
- **Live through Home Assistant** ([ADR 0009](../architecture/adr/0009-climate-via-home-assistant.en.md)):
  the `climate` section in the v1 contract snapshot and `climate.*` actions; the bridge reads
  `climate.living_room_ac`, the firmware — `transport/ha_client.cpp` (`climate_from`). Bridge loaded
  on 2026-11-03 (backup `multimedia_remote.yaml.before-climate.20261103-001829`,
  `check_config` clean, only scripts and automations reloaded, no HA restart).
  The remote received fresh air conditioning state from HA (`klima dostepna off 200`). **Commands from the remote to the
  air conditioner have not been checked yet** — each one changes a real device. Also to be checked:
  whether the Midea reports the new value within the 0.4 s after which the bridge publishes the snapshot; if not,
  the selection on the remote will briefly jump back to the old value.
- Flashed to the remote on 2026-11-03. **The first version fell into a restart loop**: the snapshot
  grew by approx. 1.1 kB, and `Z906Event` carried the whole `DeviceSnapshot` — `emit_snapshot()`
  overflowed the 6 kB stack of `mqtt_task` approx. 9 s after boot, and the Z906 client queue
  (12 slots) ate approx. 43 kB of internal RAM. Fix: `Z906Event` carries only
  `Z906State` and the revision, the snapshot is assembled by `Z906Source`/`LiveSource`. Rule for the future:
  **a transport sends only the state of its own device**, never the whole snapshot.
- After the fix: clean boot, 60 s without a restart, Z906 and HA connect. Free internal
  memory after building the screens: **124 kB** (88 kB before the fix), with live air conditioning
  (the HA event also carries `ClimateState`) — **117 kB**.
  The last recorded measurement from before the TV and air conditioning client is approx. 181 kB — the difference has not been broken down
  into parts. Air conditioning works live on the remote (checked by the user on 2026-11-03).
- Next step (agreed): review of snapshot copies and queues in internal memory
  before adding more screens.

## Simulation removed — 2026-11-03

The "Dane udawane" mode was removed from the firmware at the user's request (it was not used):
`SimulatedSource`, `SimulationControl`, the "Symulacja" (Simulation) panel in WIĘCEJ, the "Źródło stanu" (State source) row
on the MQTT page and the `Pref::Simulation` preference (the `sim` key may remain in NVS, nobody
reads it). The remote has a single source: `LiveSource`. The states from ADR 0005 (connecting, error, no
fresh state) can now be seen only on a real link — or in the HTML mockup,
whose simulation panel remains. Flashed on 2026-11-03: free internal memory after building the
screens increased from 117 to **153 kB** (queue of 4 × snapshot, a task with a 6 kB stack, state
copies and the panel).

## Start screen — design, 2026-11-03

The `docs/ux/mockup-pulpit` mockup (section "Ekran startowy" (Start screen) in the README): after boot and after every
wake — and only then — a full screen with a clock, the date and four large tiles
(Telewizor, Apple TV, Głośniki, Dom — TV, Apple TV, Speakers, Home) with 4× pixel art icons and one sentence of status.
Tapping a tile closes it until the next wake. The tab bar is unchanged, with `WIĘCEJ`
(user's decision: no START tab).

**Port to LVGL (2026-11-03, [ADR 0010](../architecture/adr/0010-start-screen-after-wake.en.md)):**
`ui/screens/screen_start.cpp`, a layer on `lv_layer_sys` above the subpages. `show_start(true)`
is called by boot (`ui::start`) and wake (`wake()` in `ui.cpp`); it then closes the subpages, the editor
and the confirmation prompt, so that after a tile is chosen they do not cover the device screen. The connecting
overlay (`render_shell`) is hidden while the start screen is open. The 96 px icons are
`icon_*_large` from `tools/gen_assets.py`, the clock — `font_b64num` (a colon was added).
The "łączenie…" (connecting…) dot is static, without the pulsing from the mockup. Flashed on 2026-11-03: clean
boot, Z906 and HA connect, 153 kB of free internal memory. The look and touch of the tiles
are being assessed by the user on the device.

The most important behaviour change: the start screen does not show control state, so it appears
**immediately after wake**, and the synchronisation (ADR 0005) happens in the background — the tiles say
"łączenie…" until the snapshot arrives. The connecting overlay appears only if
someone enters a device screen before the state arrives.

The "Ekran po starcie" (Screen after boot) setting (Więcej → Ekran, i.e. More → Display) **removed** on 2026-11-03 together with
`Pref::StartScreen` and `Pref::LastScreen` — after wake there is always the start screen,
and the TV is waiting underneath it.
- The mode segments are text-only — no function icons (see "Deliberate deviations").
  The fan speed list is a subpage with rows like the TV source list, not a bottom sheet as in the mockup.

UI code map and build recipe: [`../architecture/ui-architecture.md`](../architecture/ui-architecture.en.md).

## Memory — measurement and move to PSRAM, 2026-11-03

The boot log now has `mem:` lines (`core/memory`): free internal memory, the largest
free block, the minimum since boot and PSRAM after each stage, and after 30 s the stack headroom of every
task (`CONFIG_FREERTOS_USE_TRACE_FACILITY`).

| Stage | Before | After |
| --- | --- | --- |
| boot (after .bss) | 449 kB | 490 kB |
| shell and screens | 246 kB | 473 kB |
| network and clients | 150 kB | 385 kB |
| after 30 s (min since boot) | 160 kB (131) | 395 kB (368) |
| after 30 s, after items 2–7 | — | **439 kB (416)** |

What was done:

- **LVGL allocator in PSRAM** (`components/board/lv_mem_psram.c`, `CONFIG_LV_USE_CUSTOM_MALLOC`).
  `SPIRAM_MALLOC_ALWAYSINTERNAL=4096` kept every LVGL object in internal memory —
  five screens took 173 kB. Now 0 kB; building the interface takes as long as before (~165 ms).
  `board/CMakeLists.txt` forces the file to be linked in via `-u lv_mem_init` (link
  order).
- **Large objects in PSRAM:** `LiveSource`, `AppController`, `WifiService`,
  `NetworkController` (`main.cpp`) and `ui::Context` (`ui.cpp`) are created via
  `core::memory::make_in_psram` at the start of boot instead of in `.bss` (+41 kB). Not
  `EXT_RAM_BSS_ATTR`: an object initialised at compile time would lose its values there.

The user checked rendering smoothness with objects in PSRAM — no change.

Later the same day:

- **A single MQTT connection** (`transport/mqtt_bus.*`) for the Z906, HA and Glasshouse instead of
  three. The clients attach event handlers before `bus.start()` and filter their own
  topics themselves (including chunks of long messages: `assembly_mine_`). The v1 contract last will
  is set by the bus. Identifier: `pilot-<remote_id>` (or `pilot-<z906_id>`).
- **Sending via a dedicated task** `mqtt_tx` (stack in PSRAM) through `esp_mqtt_client_publish`,
  not `enqueue`. On the shared connection `MQTT_POLL_READ_TIMEOUT_MS=20` dropped it every ~1.5 s
  (`Network timeout while reading MQTT message`): esp-mqtt drops the connection when, during
  one loop iteration in the middle of a message, not a single byte arrives, and an iteration lasts as long as this
  timeout. The timeout went back to 1000 ms; keys do not wait for the loop iteration, because they do not go through the
  library's queue. 75 s without a disconnect. **Apple TV key latency still to be checked
  by touch.**
- **Channel menu:** a row is a button and two labels (3 LVGL objects instead of 7).
- **Log over MQTT disabled by default** (`CONFIG_PILOT_REMOTE_LOG`, a separate connection);
  `tools/pilot_log.py` works only after enabling it in menuconfig. Its task has its stack
  in PSRAM.
- **Stacks in PSRAM only for tasks that do not use flash:** `wifi_svc` stays in internal memory,
  because at boot it calls `nvs_flash_init` (PSRAM is unavailable during a flash operation).
- **Z906 input and effect lists outside the snapshot** (`domain/z906_options.hpp`): they are constant
  for the model, yet they were carried in every state copy and in the 12 slots of the client queue.

Not done from the review: building screens in the background after showing the start screen, reducing the LVGL stack
(it uses ~4 of 12 kB, but the measurement was taken without opening screens — first measure after a full
walk through the interface), remembering the Wi-Fi BSSID and channel for faster connecting.

## 48 × 48 pixel art icons — 2026-11-03

The tab and start tile icons were redrawn on a 48 × 48 grid with shading and details
(user: the old 24 × 24 ones had too little detail). Source: PNGs from `tools/draw_icons.py`
in `docs/ux/mockup-pulpit/assets/pixel/`; `tools/gen_assets.py` turns them into `I8` (previously `I4`
from the grids in `pixel-icons.js`). Flashed; clean boot. The look on the remote is being assessed by the user.

## Audio screen — 2026-11-03

- −/+ buttons on either side of the volume dial, step 1, holding repeats
  (`widgets::make_step_button`). The controller accepts odd values (HA had 23),
  even though the entity declares a step of 2 — to be checked by ear whether every step changes the level.
- Input and effect in a single row as segments with the current value
  (`widgets::make_value_segment`, shared with the TV screen), selection on a subpage with a list.
- Decode 5.1 moved to the header (a pill next to power, only on the audio screen
  and only from fresh state). Space recovered: dial 208 → 248 px (readout 64 px),
  channel columns 80 → 140 px (full range: 160 px of finger travel).

Later that day (model: the HA thermostat dial, a screenshot from the user):
- Volume dial 248 → 296 px, arc 18 → 22 px, a knob at the end of the fill (white circle
  with an accent-coloured border). −/+ (64 px) moved from the sides into the gap at the
  bottom of the arc, mute (52 px) above the number (`Dial::readout`).
- Waiting for volume confirmation no longer turns the number orange (user request); red
  stays for a failure. In the mockup the dial no longer pulses an outline.
- Air conditioning: the thermostat −/+ also sit in the arc gap (64 px), the 16°/31° scale
  moved out under the arc ends, `kThermoHeight` = 300.
- The `DOM` screen no longer has the "Snapshot · rewizja N" row — the revision is in Diagnostics.
- Tab bar: top padding 8 → 4 px (4 + 72 + 4), because there was clearly more space above
  the selected tab than below it.

## Weather on the start screen — 2026-11-03

[ADR 0011](../architecture/adr/0011-weather-on-start-screen.en.md): the bridge publishes
`weather.forecast_home` on the retained topic `multimedia_remote/v1/<id>/weather`
(`condition`, `temperature`, `night` from `sun.sun`) on change, at sunrise and sunset
and after HA starts. The remote (`HaClient::take_weather` → `LiveSource::weather_`) keeps the latest one
in memory — it is content, not state, so the start screen shows it immediately after wake, and after
6 h without an update hides it. Icons 32 × 32 → 64 px (`weather_*.png`, `icon_weather_*`).

- `night` is needed because met.no in HA reports `partlycloudy` at night as well (the user
  saw a sun at 2:16); at night the remote draws a moon behind a cloud and writes "częściowe zachmurzenie" (partly cloudy).
- Bridge loaded (backups `multimedia_remote.yaml.before-weather.20261103-021541`
  and `.before-weather-night.20261103-021757`), `check_config` clean, scripts
  and automations reloaded. The remote received the weather (`ha: pogoda: partlycloudy 55`).
- A new `.c` file in `ui/assets` requires `idf.py reconfigure` (CMake `file(GLOB)`), otherwise
  the linker will not find the image.

## More room for content — 2026-11-03

At the user's request ("it's a remote, not a phone"): header without the status bar (time and Wi-Fi
are in the settings), `kTopbar` 88 → 64; tabs with a caption only, no icons, `kTabbar`
96 → 76 (touch target 68 px). Content area 588 → 632 px, distributed as follows: TV D-pad 236 → 256 px,
channel tiles and Apple TV app tiles 88 → 100 px, trackpad 360 → 400 px, Z906 channel columns
140 → 170 px, Home scenes 104 → 120 px. Flashed, clean boot; layout to be assessed on the device.

Fixes the same day: the tab icons came back (without them it looked odd, and the gain was small) —
`kTabbar` 96 again; 5 px above the header row (`kTopbarPad`), because the power button touched
the edge; `kTopbar` = 69. At the end of the day the tab captions disappeared — icons only, 48 px,
`kTabbar` = 80 (8 + 68 + 4; since the evening of 2026-11-03 4 + 72 + 4). Content: 623 px. The Apple TV app tiles are above the trackpad,
the size of the TV channel shortcuts (80 × 100), centred.

Later that day the TV channel shortcuts and the Apple TV app tiles were reduced to an
80 × 80 square: with logos up to 38 px, a height of 100 px looked unnatural. The freed 20 px goes to
the D-pad (TV) and the spacing around the trackpad (Apple TV).

## Light and dark theme — 2026-11-03

[ADR 0012](../architecture/adr/0012-light-and-dark-theme.en.md). Two palettes with the same
13 roles (`kDark`, `kLight` in `theme.cpp`); `theme::apply()` repaints the shared and local styles of
every object at runtime, without rebuilding screens. Setting *More → Display → Style*: dark
(default) / light / by hours (17–7, 18–8 default, 19–7, 21–7), NVS `theme`, `theme_hrs`. `on_tick`
checks the style once a second, also while asleep; without a known time "by hours" gives dark. The
"Display" row in the settings shows the style (`ciemny`/`jasny`/`auto`).

- The mockup came first (`docs/ux/mockup-pulpit`, "Style" panel and a fake-hour slider), then the
  firmware.
- White logos (TVP1, TVP2, WP Pilot) disappeared on light tiles — image tiles got a fixed plate
  `theme::logo_plate()` (`0x16181d`), outside the palette.
- Pitfall: `apply()` recognises a role by colour value. Two roles with the same colour, or a fixed
  colour equal to a role of either palette, will be repainted wrongly.
- Flashed and checked on the device in both styles; screenshots in `docs/screenshots/` (dark) and
  `docs/screenshots/light/`. The remote was left in the dark style.
- `tools/remote.py tour` had captured the start screen six times since ADR 0010 (it covers the tabs);
  it now taps the TV tile after the start shot, which only closes the start screen.

## Remote log over MQTT (during development)

`CONFIG_PILOT_REMOTE_LOG` ("Pilot" menu, enabled by default) copies every ESP_LOG line
to the topic `pilot_debug/<remote-id>/log` on the broker from the MQTT settings. Lines from before
the connection wait in a 32 kB buffer in PSRAM, so the boot is visible too. The log client starts only
when the Wi-Fi service brings up the TCP/IP stack — starting earlier ended in an lwIP assertion
("Invalid mbox") and a restart loop.

Reading via the Home Assistant websocket (the broker requires login, HA has access):

```bash
HA_URL=http://<ha-address>:8123 HA_TOKEN=... uvx --with websockets python tools/pilot_log.py + 60
```

The log contains no secrets: network names, passwords and addresses are not logged, the `eh_wifi` tag
(ESP-Hosted printed the network name) is limited to warnings. Disable before release.

The HA client logs every received snapshot (revision, Apple TV availability, number of apps)
and the loss of the connection to the broker — in order to distinguish "HA is silent" from "HA says: unavailable".

## USB console: screenshots and remote touch (during development)

`CONFIG_PILOT_USB_CONSOLE` ("Pilot" menu, enabled by default) installs the
USB-Serial-JTAG driver and listens on it for the commands `shot`, `tap x y` and `drag x1 y1 x2 y2 ms`.
`shot` copies the DPI panel frame buffer (what is visible, including overlays from `lv_layer_top`
and `lv_layer_sys`) and sends it back in base64; ESP_LOG goes quiet while it is being sent. Touch goes through a separate
LVGL input alongside the GT911. Usage from a computer:

```bash
uvx --with pyserial --with pillow python tools/remote.py shot screen.png
uvx --with pyserial --with pillow python tools/remote.py tour docs/screenshots/
```

About 2.5 s per screenshot, without restarting the remote. `tour` touches only the tab bar and the TV tile on the start screen — remote
touch on a control really does control the home. The settings screen shows the Wi-Fi network name: before
publishing a screenshot it must be masked. Documentation screenshots are taken in both styles
(`docs/screenshots/` and `docs/screenshots/light/`, plus `overview.png` and `overview-light.png`)
and in both orientations (`docs/screenshots/landscape/` and `landscape/light/`, plus
`overview-landscape*.png`). The Home tab has its own set (`05-home`, `10-room`…`14-drawers`) and `overview-home*.png` — see `docs/ux/home-screen.en.md`, section 6; overviews are composed by `tools/overview.py`. `tour` only knows portrait; landscape is captured by hand (`shot`, `tap`
on the side-rail tabs x = 44, y = 64/152/240/328/416). Remote touch in landscape takes logical
800 × 480 coordinates. Take a screenshot before a series of taps: after the screen blanks the remote
returns to the start screen and taps land elsewhere.

Pitfall: macOS raises DTR and RTS when the port is opened, and USB-Serial-JTAG resets the chip on
RTS=1 and DTR=0 — RTS must be lowered before DTR (the script does this). A plain port open
with `dtr=False, rts=False` set before `open()` restarts the remote, and the screenshot then captures
a frame from the boot.

## Landscape orientation — 2026-11-06

[ADR 0013](../architecture/adr/0013-landscape-orientation.en.md), mockup
`docs/ux/mockup-landscape/`. Setting *Display → Orientation* (portrait / landscape / landscape
flipped), NVS `orient`, restart after a change. Every screen and sub-page has a landscape layout;
patterns in `ui-architecture.en.md` ("Landscape orientation").

- Rotation is done on the CPU (`sw_rotate`). PPA disabled: it hung the UI while scrolling.
- The MQTT client waits for an IP address (`MqttBus::start`). It used to connect before Wi-Fi and
  every boot showed "no connection to home" for a few seconds.
- On weak Wi-Fi (−90 dBm, reason 201) the connection can take a minute; the overlay then shows the
  MQTT step although it is waiting for Wi-Fi — to fix.
- Climate mode icons are five Lucide 0.460.0 glyphs (`lucide_modes_24/36.c`,
  `tools/gen-fonts.sh` with `LUCIDE_TTF`).
- Power: 48 px circle, 64 px touch area (`ext_click_area`) — the parent must be 64 px tall.
- Not checked on the board: finger touch in the new layouts, 270° rotation, the −/+, mode and power
  buttons on the climate card.

## Rooms and lights — 2026-11-07

[ADR 0014](../architecture/adr/0014-rooms-and-lights.en.md), standard
[`docs/ux/home-screen.en.md`](../ux/home-screen.en.md). Mockups: `docs/ux/mockup-pulpit`,
`docs/ux/mockup-landscape`, shared `docs/ux/mockup-dom/rooms.js` and `rooms.css`.

- Home: scenes removed, a six-room grid (`ui/screens/screen_home.cpp`), A/C as a strip
  (portrait) or a header pill (landscape). The large A/C tile is no longer built;
  `render_climate` only draws the subpage, `open_climate_page` opens it from the room card.
- Room page: full screen on `lv_layer_sys`. In landscape both rails live in drawers
  (`build_drawers`): a 28 px edge strip catches the swipe (`PRESSING`, ≥ 36 px) and taps.
- Light screen like the A/C subpage: the brightness dial is `widgets::Thermostat` in percent
  mode (`set_percent`, 5–100, step 5), tiles for colour temperature, effect and bulbs. Effects
  are a picker (`dock_sheet`), choosing closes it. Power in the header on the right.
- Icons: `tools/draw_room_icons.py` (rooms 48, lamp and bulb 32) → `gen_assets.py`; effect icons
  `lucide_effects_22` (`tools/gen-fonts.sh`).
- Contract: `lights` section, `light.*` actions; `HaEvent` is about 3 kB, its queue is in PSRAM.
  The bridge is deployed on the HA server (backup `…before-lights.20261107-073712`).
- Lesson: a screenshot does not wake the screen, it shows the last frame. Check `hit` before
  every tap — an accidental tap on the Apple TV screen launched YouTube and (via CEC) the TV.

## Start screen: at home — 2026-11-07

[ADR 0015](../architecture/adr/0015-indoor-average-on-start-screen.en.md). The weather message has an
`indoor` field (average temperature and humidity of the room sensors, computed by the bridge;
backup `…before-indoor.20261107-091007`). Landscape: a 96 px clock (`ubuntu_b_96_num`) and the date
centred in the left column, "outside" / "at home" tiles; portrait: a pill under the weather.
Start tile icons are centred. Checked on the board in landscape; portrait only in the mockup.

## UI refactor — 2026-11-08

No change in look or behaviour (screenshots of 9 landscape screens before and after are
pixel-identical). Shared LVGL primitives in `widgets` (`make_box`, `make_text`,
`make_bare_button`, `make_dot`, `make_chevron`, `set_visible`, `upper_pl`) replace copies in
screen files; `make_intent(kind, control)` in `ui_internal.hpp`. The Home tab is split into
`screen_home.cpp` and `home_*.cpp` with `home_internal.hpp`, the A/C into `screen_climate.cpp`
(subpage) and `climate_tile.cpp` (tile) with `climate_internal.hpp`. `build_start` is split into
parts. UI compiler warnings removed (`theme::selector`, `theme::kOpaTransp`/`kOpaCover`).
Portrait was not checked on the board after the refactor — the changes touch code, not layout.

## Next safe step

1. The WIĘCEJ screen according to the plan above (stage 2).
2. TV: the LG integration in Home Assistant and the `tv` section — the HA client is already there, it just needs to read it.
3. Real power saving, not just turning off the backlight: requires confirming the
   hardware touch wake-up path and accepting a longer wake-up time.
4. Only then the topics requiring ADRs: the Emby library, posters over HTTP.

Before hardware code or flashing anything, you must:

1. check `git status --short --branch` in both repositories;
2. read `AGENTS.md`, `docs/architecture/README.md`, `docs/architecture/mqtt-contract.md` and `docs/hardware/bring-up.md`;
3. obtain explicit permission to write to the board;
4. after every significant stage, update this document: date, commit, what was confirmed, what remains unconfirmed and the next safe step.
