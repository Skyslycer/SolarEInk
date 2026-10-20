# AGENTS.md — SolarEInk firmware

ESP32-S3 SuperMini firmware for the **296 × 128 black/white/red WeAct
display**. Each cycle reads the local Fronius Solar API and today's
Open-Meteo forecast, measures its own LiPo, renders the dashboard,
hibernates the panel, and deep-sleeps.

This file is the working guide for coding agents. The human-facing README is
maintained separately by the project owner; keep technical facts here accurate
and do not treat this as the public README.

## Quick start

| Task | Command |
| --- | --- |
| Build | `pio run` |
| Flash | `pio run --target upload` |
| Follow serial | `./monitor` |
| Render previews | `scripts/preview` |
| Regenerate font header | `python3 scripts/generate_fonts.py` |

Host checks (run after PlatformIO has installed ArduinoJson):

```sh
c++ -std=c++11 -Wall -Wextra -pedantic -Iinclude \
  -I.pio/libdeps/esp32-s3-supermini/ArduinoJson/src \
  test/test_readings.cpp -o /tmp/solareink-test-readings
/tmp/solareink-test-readings
c++ -std=c++11 -Wall -Wextra -pedantic -Iinclude \
  test/test_logic.cpp -o /tmp/solareink-test-logic
/tmp/solareink-test-logic
```

`test/`, `tools/`, and `previews/` are gitignored and may only exist locally.
`platformio.ini` targets `esp32-s3-supermini` (generic S3, 4 MB flash, no PSRAM
init, native USB CDC on boot).

## Ground rules

- **Secrets stay in `include/solar_config.h`** (gitignored). Never print,
  commit, quote, or paste the Wi-Fi password or `INFLUX_TOKEN`. Mirror new
  config keys in `include/config.example.h`.
- **The InfluxDB and Docker host are the owner's real infrastructure.** Do not
  modify Docker, networks, or buckets. Do not leave test points behind; if a
  write test is unavoidable, delete the test measurement immediately with the
  v2 delete API.
- **Coordinate flashes with the owner.** Their monitor normally holds the
  serial port, and the board is only awake ~25–30 s per 15-minute cycle.
- **Bump `App::FIRMWARE_VERSION`** in `include/app_config.h` for every flashed
  change — it is the `fw` telemetry tag.
- **Do not re-add the telemetry probe/backoff.** Telemetry is deliberately one
  POST attempt per cycle with a single RTC-staged record (see below). Retries
  on a dead server cost a full 5 s connect timeout each; the meaningful retry
  is the next cycle.
- **Preserve uncommitted work.** Inspect `git status` before deleting or
  overwriting. Match the existing style, `Config::`/`Timing::` constants, and
  snake_case field names.

## Flashing and USB quirks

- The build images are **DIO**. `pio run --target upload` is the safe path.
  Do **not** manually flash with `--flash_mode qio`: the ROM then loads the
  wrong SPI mode and the board enters a watchdog reset loop
  (`rst:0x7 TG0WDT_SYS_RST`).
- Manual esptool fallback, after entering download mode (hold **BOOT**, tap
  **RST**, release **BOOT**): use the PlatformIO venv python
  (`$HOME/.platformio/penv/bin/python`), `--before no_reset`, `--flash_mode
  dio --flash_freq 80m --flash_size 4MB`, and offsets `0x0` bootloader,
  `0x8000` partitions, `0xe000` boot_app0, `0x10000` firmware. The packaged
  `esptool.py` fails under the system python (missing `intelhex`).
- If the board stays in download mode after flashing, tap **RST** with
  **BOOT** released.
- Native USB Serial/JTAG: keep **DTR and RTS asserted** when opening the
  port. Clearing either individually resets the chip and loses RTC state.
  `scripts/monitor.py` handles this; prefer `./monitor` over
  `pio device monitor`.
- Deep sleep disconnects USB; the port may vanish and re-enumerate.
  `./monitor` follows the board by USB serial number. Close other monitors
  first; Ctrl+C exits.
- The first boot after flashing is a cold boot: `Metrics::resetPending()`
  clears telemetry staging, so expect no Influx point from that cycle.
- Adding or removing any `RTC_DATA_ATTR` variable shifts the `.rtc.data`
  layout. On the next cold boot, reset any state that depends on the old
  layout; never trust RTC bytes across a reflash.
- `SERIAL_WAIT_MS` (default 0) can add grace for a late host attach at the
  cost of awake time. Early boot lines can still be missed when the host
  attaches late.

## Project layout

- `src/main.cpp`: wake → ADC → network → render → minute scheduling → sleep.
- `src/network.cpp`: Wi-Fi (cached AP), bounded HTTP requests, API parsing
  diagnostics.
- `src/metrics.cpp`: RTC-held telemetry record and InfluxDB line protocol.
- `src/battery.cpp`, `src/clock_sync.cpp`: LiPo ADC and NTP/RTC time.
- `src/dashboard.cpp`: layout and text; `src/icons.cpp`: icons and flow arrows;
  `src/display.cpp`: panel driver.
- `include/logic.h`: formatting, voltage curve, weather groups, time boundaries.
- `include/app_config.h`: pins, timings, display, firmware version.
- `include/solar_config.h`: real Wi-Fi/Fronius/Influx config (gitignored).
- `include/config.example.h`: template; keep keys in sync.
- `scripts/monitor.py`: serial follower; root `monitor` wraps it with the
  PlatformIO Python environment.
- `scripts/preview`, `scripts/render_preview.cpp`: host renderer.
- `scripts/generate_fonts.py`: generates `include/fonts/dashboard_fonts.h`.

## Hardware

The module's SDA/SCL labels refer to **SPI MOSI/SCK**, not I²C.

| Module connection | ESP32-S3 connection |
| --- | --- |
| SDA / MOSI | GPIO6 |
| SCL / SCK | GPIO1 |
| CS | GPIO5 |
| D/C | GPIO2 |
| RES | GPIO4 |
| BUSY | GPIO3 |
| GND | GND |
| VCC | Existing 5 V module supply |
| Battery ADC | GPIO8 |

The ADC divider is `LiPo+ → 220kΩ → GPIO8 → 220kΩ → GND`, with **100nF from
GPIO8 to GND**. Measurement uses 32 calibrated millivolt samples before Wi-Fi
or display initialization, a 60 ms settling delay, and a trimmed mean. The
divider factor is 2. The small top-right percentage is an approximate rested
LiPo voltage-to-SOC curve; charging, load, temperature, resistor tolerance,
and ADC error affect it. `BATTERY_CALIBRATION` can correct the voltage against
a meter. This percentage is separate from the **Fronius storage SOC** at
bottom right.

The driver is `GxEPD2_290_C90c` (GDEM029C90 / SSD1680), matching
[WeAct's three-color example](https://github.com/WeActStudio/WeActStudio.EpaperModule/blob/master/Example/EpaperModuleTest_Arduino_ESP32/EpaperModuleTest_Arduino_ESP32.ino).
Landscape rotation is 1; set `Hardware::DISPLAY_ROTATION` to 3 if upside down.

## Configuration

The real Wi-Fi and location settings are in `include/solar_config.h`, which is
ignored by Git. For a fresh checkout, copy `include/config.example.h` to that
path and fill in the SSID, password, inverter IP, and coordinates.

- `REFRESH_SECONDS` defaults to **900** and is the target screen-update
  interval, aligned to quarter-hour wall-clock marks (13:00, 13:15, 13:30…).
  It is not a raw sleep duration and must be a multiple of 60.
- `FIRST_UPDATE_HOUR` (default **6**) and `LAST_UPDATE_HOUR` (default **23**)
  bound the local-time update window; the last update is the 23:00 mark.
- The overnight sleep is split by a time-sync-only checkup at `CHECKUP_HOUR`
  (default **5**) and then sleeps until `MORNING_WAKE_BEFORE_MINUTES` (default
  **1**) before the 06:00 update, i.e. 05:59.
- Pins, ADC, timezone, NTP, and API timing live in `include/app_config.h`.
  Influx settings (`INFLUX_URL`, `INFLUX_ORG`, `INFLUX_BUCKET`, `INFLUX_TOKEN`,
  `DEVICE_NAME`) live in the same file; leave `INFLUX_URL` empty to disable
  telemetry.
- Use a 2.4 GHz Wi-Fi network that can reach the inverter.

## Behavior

### Display

- Top left: today's min/max °C, maximum precipitation probability, and an icon
  selected from all documented Open-Meteo WMO codes (sun, cloud, fog, drizzle,
  rain, freezing precipitation, snow, showers, thunderstorms, hail).
- Top center: solar production and the sun/panel icon.
- Top right: Wi-Fi strength/offline indicator, device LiPo percentage, and
  local `HH:MM`. Timezone defaults to Europe/Berlin's CET/CEST rules with DST.
- Middle: house consumption, always red and shown as an absolute value.
- Bottom left: grid magnitude and arrow toward the house for import, away for
  export. Backup mode replaces this section with white **NOTSTROM AKTIV** on
  red and an open-switch symbol toward the house.
- Bottom right: Fronius battery SOC and four dynamic red bars. Discharge points
  toward the house with a red up arrow beside power; charge points away with a
  black down arrow.
- Power fields switch from W to kW. Units are smaller than digits; kW precision
  drops to one decimal where space is tight, then a smaller numeric font if
  needed. Missing/invalid data shows `--`; it is never silently zero or
  retained as fresh. Zero flows have no directional arrowhead.
- Icons use native pixel resolution. Grid and battery connections share a
  horizontal centerline and mirrored arrowheads; solar and battery power
  arrows use the same shape vertically. Pylon, panel cells, Wi-Fi arcs, and
  battery outlines have symmetric geometry.

### Data sources

- Open-Meteo requests one day's four daily fields, with `timezone=auto` and
  Celsius, over plain HTTP.
- Fronius is queried at `/solar_api/v1/GetPowerFlowRealtimeData.fcgi` with the
  previously verified installation-specific signs.
- Neither endpoint failure prevents the other request or the device returning
  to sleep.
- Each API request gets up to ten attempts with a 250 ms pause between
  failures. Long outages can extend awake time considerably; weather polling
  still runs after Fronius retries finish.

### Wi-Fi

- The firmware calls standard `WiFi.begin()`. A successful cycle caches the
  AP's BSSID and channel in RTC memory; the next connect targets that AP
  directly with a bounded six-second attempt, skipping the scan. On failure
  the cache is cleared and the driver scans all channels, preferring the
  strongest AP when several share the SSID.
- One initial connection attempt plus three retries, each with a 15-second
  timeout and a 500 ms pause.
- On Wi-Fi failure, Serial reports the disconnect reason and whether
  association ever succeeded. It never prints the password. A disconnected
  status alone does not identify a bad password.

### NTP and scheduling

- NTP syncs on first connection and every six hours thereafter. Each sync
  allows four attempts (one plus three retries), each with an eight-second
  timeout and a 500 ms pause.
- The ESP32's RTC-backed system clock keeps time through deep sleep. Each
  screen is labelled with its intended **completion** minute.
- The next wake is scheduled early enough for ADC, Wi-Fi, polling, and refresh;
  actual preparation and refresh lengths are retained in RTC memory to adjust
  the next cycle.
- Wi-Fi turns off before any alignment wait or screen update.
- A completion up to `TARGET_LATE_TOLERANCE_MS` (two minutes by default) after
  a boundary still refreshes with that boundary's minute. Later targets skip
  the interval and sleep through to the next one. Targets outside the daily
  window are skipped the same way.
- The quiet hours are two sleeps: a 05:00 Wi-Fi/NTP-only checkup (no fetch, no
  screen update) to correct the RC clock's overnight drift, then sleep to
  ~05:59 for the normal 06:00 cycle.
- A cold boot has no carried target and sleeps through to its first aligned
  update. If time has never synchronized, the clock shows `--:--` and updates
  use relative timing until NTP succeeds.

### Refresh timing and power

- The three-color panel full update is approximately **27 seconds** (per the
  GxEPD2 driver). Partial updates still perform a full-screen waveform, so
  they do not save refresh time; there is no supported fast clock-only partial
  refresh. Visible flashing during the waveform is expected.
- When no USB host is attached, the CPU light-sleeps through the alignment wait
  before the refresh and, in 40 ms slices via GxEPD2's busy callback, through
  the 27-second waveform (about 22 mA → 1 mA for those phases). With a monitor
  attached it stays awake so bench behavior matches the old firmware.
- The display is explicitly hibernated before deep sleep.
- Schedule: 69 refreshes/day (06:00–23:00 every 15 min) plus the 05:00
  checkup. Bench estimate is roughly 10–13 mAh/day; an untethered
  full-charge drain test is still the authoritative measurement.
- Telemetry adds ~1.2 kB/point (~90 kB/day) and roughly 0.2–0.4 mAh/day inside
  an already-open Wi-Fi window.

## InfluxDB telemetry (optional)

Fill in `INFLUX_URL` (server base URL, e.g. `http://192.168.1.50:8086`),
`INFLUX_ORG`, `INFLUX_BUCKET`, and `INFLUX_TOKEN` in
`include/solar_config.h` to log one point per cycle to measurement `solar`.
Tags are exactly three: `name` (`DEVICE_NAME`), `mac`, and `fw`
(`App::FIRMWARE_VERSION`). Leave `INFLUX_URL` empty to disable.

The cycle only knows its refresh duration and completion error after the panel
finishes, which is after Wi-Fi is normally off. The record is completed just
before deep sleep, kept in RTC memory, and posted on the **next** cycle's
Wi-Fi window, right after the API fetches. The point timestamp is the recorded
cycle's wake time from the device clock (server time only when the clock was
never synchronized), so intervals and sleep durations are derivable from
consecutive timestamps. The first cycle after flashing or a hardware reset has
no predecessor and posts nothing.

The write is one HTTP/1.0 `POST /api/v2/write` (line protocol, second
precision) with five-second connect and read timeouts. It is one attempt only;
a failed POST is recorded in `prev_post_*` and the next cycle tries again.
There is a single pending slot: a failed or Wi-Fi-less post is dropped when the
next cycle stages its record. `src/metrics.cpp` is the authoritative field
list.

Schema rules: only the three tags above; everything else is a field. Do not add
derived or never-changing values. BSSID and IP stay fields. Do not add NTP
server names, forecast date/timezone, core version, URLs, or coordinates.

Field groups (snake_case, units in the name):

- scheduler/boot: `boot_count`, `wake`, `reset`, `checkup`, `usb`, `plan`,
  `target`, `missed_ms`, `completion_err_ms`, `awake_ms`
- battery: `device_v` (device LiPo volts), `device_pct`
- Wi-Fi: `wifi_ok`, `wifi_ms`, `wifi_cached`, `wifi_fallback`, `wifi_attempts`,
  `wifi_status`, `wifi_reason`, `rssi`, `channel`, `bssid`, `ip`
- NTP: `ntp_ran`, `ntp_ok`, `ntp_attempts`, `ntp_ms`, `ntp_drift_ms`,
  `sync_age_s`, `clock_ok`
- Fronius: `fr_*` including `fr_pv_w`, `fr_load_w`, `fr_grid_w`, `fr_batt_w`,
  `fr_soc` (house battery SOC, not the device), `fr_backup`
- Open-Meteo: `wx_*` including `wx_tmin_c`, `wx_tmax_c`, `wx_precip_pct`,
  `wx_code`
- panel/light sleep: `prep_ms`, `refresh_ms`, `wait_sleeps`, `wait_slept_ms`,
  `ref_sleeps`, `ref_slept_ms`, `sleep_fails`
- system: `chip_temp_c`, `heap_free`, `heap_min`, `heap_max`
- telemetry meta: `prev_post_ok`, `prev_post_ms`

Missing values are omitted instead of written as zero; an absent field means
"not measured", not "zero". The first NTP sync after a cold boot can report a
huge `ntp_drift_ms` because the RTC clock was cleared; later syncs are normal.

## Fonts and previews

- Text uses original 5 × 7 pixel glyphs, enlarged only by whole-number
  multiples in both dimensions. No downscaled or stretched outline font.
- After editing glyphs, regenerate the bundled header with
  `python3 scripts/generate_fonts.py`.
- `scripts/preview` executes the **same dashboard drawing code and Adafruit
  GFX rasterizer** used on the ESP32; no hardware needed. Requires a C++
  compiler and Python Pillow. PNGs appear in `previews/` (normal, backup, all
  four grid/battery flow combinations, zero flow, large kW, unavailable data,
  every supported weather code). Native previews are exactly 296 × 128; `-2x`
  and `-4x` enlarge pixels without smoothing.
  `previews/flow-directions.png` groups the four flow combinations.

## Definition of done

- `pio run` finishes cleanly; both host checks pass.
- `scripts/preview` renders without errors when layout, icons, or fonts change.
- Flash verified on hardware; serial shows the expected wake/cycle with no
  resets or panics, and the panel refreshes.
- Telemetry: the next cycle's `Influx: HTTP 204` line appears and the point is
  present with the expected fields. No test points remain in the bucket.
- `App::FIRMWARE_VERSION` bumped for any flashed change.
- Documentation keys mirrored in `include/config.example.h` when config
  changes.
