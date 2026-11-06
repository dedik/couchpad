# Verified board map

[Polski](verified-board-map.md)

Values confirmed **on this particular unit**, not copied from similar models. The procedure and
criteria come from [`bring-up.md`](bring-up.en.md).

- **Date:** 2026-10-29 (hardware), 2026-10-30 (UI firmware)
- **Board:** Guition JC4880P443C_I_W_Y
- **Test method:** custom `pilot_bringup` program flashed over USB; reading the boot log from the
  serial port and visually inspecting the screen and touch. On 2026-10-30 the same for the actual
  firmware from the repository (`idf.py flash`, five Pulpit screens on simulated data)

## Confirmed on this unit

| Parameter | Value | How we know |
| --- | --- | --- |
| Chip | ESP32-P4 **rev v3.2**, 2 cores + LP, 400 MHz | `esptool chip-id` and the boot log |
| Flash | 16 MB, manufacturer 0x68, device 0x4018, QIO mode 80 MHz | `esptool flash-id` |
| Flash controller | Boya — IDF uses the generic driver and warns about it | boot log |
| PSRAM | **32 MB**, HEX mode, 200 MHz; ~33.0 MB free | boot log |
| Panel | 480 × 800, RGB565, MIPI-DSI, 2 data lanes | boot log + image on screen |
| Panel driver | ST7701 with the vendor's **patched** sequence | BSP code + working image |
| Touch | **GT911**, I2C, responds: `TouchPad_ID 0x39,0x31,0x31` ("911"), configuration v250 | boot log |
| Full surface | frame visible on all four edges | visual inspection |
| Touch in corners | all four corners hit, readings up to `X 459 Y 774` | visual inspection |
| Stability | no restarts and no artefacts after the clock fix | boot log |
| Colours | correct — amber is amber, background almost black; RGB channel order correct | live inspection |

## Pins in use and working

They come from the vendor's BSP for this model; the screen and touch came up on them.

| Function | GPIO |
| --- | --- |
| I2C SDA / SCL (touch, codec) | 7 / 8 |
| Backlight (LEDC) | 23 |
| Panel reset | 5 |
| Touch reset and interrupt | not connected |
| I2S MCLK / BCLK / LRCK / DOUT / DIN | 13 / 12 / 10 / 9 / 48 |
| Amplifier enable | 11 |
| microSD D0–D3 / CMD / CLK | 39, 40, 41, 42 / 44 / 43 |
| MIPI-DSI PHY power | internal LDO, channel 3, 2500 mV |
| microSD power | internal LDO, channel 4 |

Image parameters from the patched component: DPI clock 34 MHz, lane bit rate 750 Mbps, porches
42/12/42 horizontal and 8/2/166 vertical.

## Pinned versions

| | |
| --- | --- |
| ESP-IDF | **v5.5.6** |
| LVGL | **9.2.2** |
| esp_lvgl_port | **2.6.0** |
| esp_lcd_touch_gt911 | **1.1.3** |
| esp_lcd_st7701 | **1.1.3**, copy patched by the vendor, kept in the repository |

These versions **must not be loosened without a test on the hardware**. `esp_lvgl_port` 2.9 uses the
constant `LV_COLOR_FORMAT_RGB565_SWAPPED`, which was only added in LVGL 9.3, so it does not compile
at all with LVGL 9.2.2.

## Fixes required on this unit

**PLL clock source for the MIPI-DSI PHY.** The vendor's BSP sets
`.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT`, a compatibility constant meaning `PLL_F20M`. For
chips from revision 3.0 upwards ESP-IDF compiles a HAL variant that knows only XTAL and four PLLs,
so `PLL_F20M` falls into `default: abort()`. The result: a restart loop before the screen even
gets a chance to light up.

Fix: **do not set this field at all**. `esp_lcd_new_dsi_bus()` picks the source for the chip
revision by itself, which works both on older boards and on ours.

The diagnosis came from decoding the crash address, not from trial and error:
`_mipi_dsi_ll_set_phy_pllref_clock_source` in `hal/esp32p4/include/hal/mipi_dsi_ll.h`.

## UI firmware pitfalls

Confirmed on 2026-10-30 on the actual firmware from this repository.

**LVGL memory pool.** LVGL's built-in allocator gets `LV_MEM_SIZE_KILOBYTES = 64` by default.
Five Pulpit screens built at once amount to several hundred objects, and this pool cannot hold
them. Once it is exhausted, `lv_malloc` **does not report an error — it loops**, searching for a
free block. The symptom is misleading: the `main` task eats core 0, the watchdog fires every 5 s on
`IDLE0`, and the crash address lands in the ROM `memcpy`, so it looks like a chip fault rather than
running out of memory. The hang location moves with every code change, because the pool runs out
wherever it happens to.

Fix: hand allocation over to the ESP-IDF heap, which reaches into PSRAM.

```
CONFIG_LV_USE_CLIB_MALLOC=y
CONFIG_LV_USE_CLIB_STRING=y
CONFIG_LV_USE_CLIB_SPRINTF=y
CONFIG_SPIRAM_USE_MALLOC=y
```

After this change, with five screens built, **442 kB of internal memory and 31 938 kB of PSRAM**
remain. The application is 1.13 MB in a 7 MB partition.

**Linking the patched ST7701 driver.** The vendor's BSP manifest pointed to the path
`../espressif__esp_lcd_st7701`, which does not exist in this repository — written that way, the
patched copy never made it into the build at all. The driver is now wired in via `PRIV_REQUIRES
esp_lcd_st7701_patched` in `components/board_bsp/CMakeLists.txt`. In addition, `cu_pkg_define_version`
builds macro names from the directory name, so in `components/esp_lcd_st7701_patched/CMakeLists.txt`
we substitute `ESP_LCD_ST7701_VER_*` for `ESP_LCD_ST7701_PATCHED_VER_*`. The vendor's sources remain
unchanged.

**Frame pointer.** `CONFIG_ESP_SYSTEM_USE_FRAME_POINTER=y` bloats the bootloader to 0x6650 against a
limit of 0x6000 and the build fails. To diagnose hangs, use staged logs or move the partition
table.

## Messages that look like errors but are not

- `Install EK79007 LCD control panel` — **misleading**. A few lines below, the same block calls
  `esp_lcd_new_panel_st7701()`. The vendor reworked the contents of the branch but left the old
  message.
- `swap_xy is not supported by this panel` — the BSP tries to rotate the image in hardware, which a
  DPI panel cannot do. With the native 480 × 800 portrait orientation this is irrelevant.
- `GPIO 23 is not usable, maybe conflict with others` — double reservation of the pin with LEDC.
  Brightness is still set to 100% without error, and the screen lights up.
- `Detected boya flash chip but using generic driver` — works; `SPI_FLASH_SUPPORT_BOYA_CHIP` can be
  enabled later.

## Not yet confirmed

- **Touch reset.** The BSP says "not connected", some community sources point to GPIO 3.
  Touch works without it, so the matter is open and non-blocking.
- **Power consumption and power-saving modes** — untested.
- **Finger touch in landscape.** 90° rotation (Display → Orientation, `sw_rotate` on the CPU)
  confirmed 2026-11-06: 800 × 480 image the right way up, remote touch from `remote.py` lands.
  GT911 finger touch and 270° rotation still need a manual check.
- **PPA for rotation.** With `CONFIG_LVGL_PORT_ENABLE_PPA=y`, scrolling a list in landscape hung
  the LVGL task for good (2/2 attempts): it waited on the PPA holding the lock, the watchdog stayed
  quiet. CPU rotation works; the PPA root cause is unknown (ADR 0013).

## ESP32-C6 and Wi-Fi

Confirmed on 2026-10-30. The ESP32-P4 has no radio of its own: Wi-Fi is provided by the ESP32-C6
over SDIO, via `espressif/esp_wifi_remote` (the `esp_wifi_*` API) and `espressif/esp_hosted`
(transport).

| Parameter | Value | How we know |
| --- | --- | --- |
| Bus | SDIO, 4-bit, 40 MHz, slot 1 | `eh_sdio` log |
| SDIO pins CLK / CMD / D0–D3 | 18 / 19 / 14, 15, 16, 17 | `eh_sdio` log |
| Coprocessor reset | GPIO **54** | `eh_sdio` log |
| Coprocessor | ESP32-C6, chip id `0x0d` | `eh_init_evt` |
| Firmware on the C6 | **2.3.2**, factory, untouched | `eh_init_evt` |
| Host | `esp_hosted` **3.0.10**, `esp_wifi_remote` **1.6.5** | `eh_init_evt` |
| Mode | WLAN over SDIO, RPC V2, streaming | `eh_init_evt` |

**Nothing needs to be flashed to the C6.** The factory 2.3.2 coprocessor gets along with host
3.0.10: negotiation ends with `Default RPC to V2` and `compatible streaming mode enabled`. Two
warnings in the log are harmless and follow directly from the version difference:

- `CP without SDIO SW_AGGR; compatible streaming mode enabled` — the coprocessor cannot do
  aggregation, so the host falls back to compatible mode;
- `Missing SDIO_MODE TLV; Continue anyway.` — older firmware does not report this field.

We **do not specify the pin configuration manually**: `CONFIG_ESP_HOSTED_P4_DEV_BOARD_FUNC_BOARD=y`
selects it automatically and it matches this board. The vendor's example does the same and, in
fact, sets nothing at all in this respect.

The link comes up about 1.2 s after the screen starts and does not delay the UI — `esp_hosted`
initialises before `app_main`, but handles the rest in its own task. Confirmed along the full
path: scan from the screen, network selection, password from the keyboard, save to NVS, and
**automatic connection after a restart at 7.4 s from boot**, with the screen ready at 2.2 s.

### Two Wi-Fi API pitfalls via the coprocessor

**Blocking scan does not work.** `esp_wifi_scan_start(cfg, true)` over RPC returns after about
100 ms with zero results. The scan has to be requested non-blocking and then you wait for
`WIFI_EVENT_SCAN_DONE`.

**The coprocessor has its own NVS.** After `esp_wifi_start()` the C6 tries on its own to connect to
the network remembered by the factory demo, and then rejects the scan with `ESP_ERR_WIFI_STATE`.
After start you have to call `esp_wifi_disconnect()` and retry the scan. We do not erase that
data — the remote keeps its own in NVS on the P4 side and sets `WIFI_STORAGE_RAM`, so it does not
overwrite someone else's entry.

**A scan can drop the connection.** This applies both to the workaround for the previous pitfall
(`esp_wifi_disconnect()` before retrying the scan may only be called when there is no link
anyway) and to scanning through the coprocessor itself. That is why we **do not scan
automatically** on entering the Wi-Fi screen: with a working connection the list stays empty, and
a scan is only started by tapping the button. After such a deliberate scan, reconnecting to the
network is immediate, without the delay intended for a router that has been switched off.

In addition, the service phase changes for the duration of the scan, so the connection state has
to be kept in a separate field: otherwise, after every scan the UI reports no network even though
it is up.

As a side note: `esp_hosted` writes the network name to the log at its own log level. Our code
logs only the fact of connecting and the signal strength. Dumps containing the network name are
not pasted into the repository.

## Backup and restore

The full 16 MB image from before flashing is at `backups/guition-esp32p4-before-ui-2026-10-29.bin`,
with a SHA-256 matching the record in `../operations/ai-handoff.md`. Restoring the factory state:

```bash
esptool --port /dev/cu.usbmodem101 write-flash 0x0 backups/guition-esp32p4-before-ui-2026-10-29.bin
```

The flash **was not erased** — only the bootloader, partition table and application were
overwritten. The factory layout differed from ours: table at 0x9000, 7 MB application at 0x20000,
6 MB spiffs at 0x720000.
