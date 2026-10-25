# AGENTS.md — SolarEInk firmware

Agent guide for the ESP32-S3 SuperMini firmware and 296 × 128 black/white/red
WeAct display (GDEM029C90 / SSD1680). The owner maintains `README.md` as the
public build guide. Keep changes scoped to the task and preserve uncommitted
and untracked work, including documentation, images, and CAD files.

## Ground rules

- Inspect `git status` before editing or deleting files.
- Secrets belong in gitignored `include/solar_config.h`. Never print, commit,
  quote, or paste the Wi-Fi password or `INFLUX_TOKEN`, including in command
  output or error logs. Mirror configuration changes in `include/config.example.h`.
- The InfluxDB/Docker host is real infrastructure. Read-only telemetry queries
  may use configured credentials without exposing them. Do not modify Docker,
  networks, or buckets. Avoid test writes; if an authorized write test is
  unavoidable, immediately delete its test measurement with the v2 delete API.
- Coordinate flashes with the owner: their monitor may hold the serial port,
  and deep sleep disconnects USB. Bump `App::FIRMWARE_VERSION` in
  `include/app_config.h` for every changed firmware that is flashed.
- Preserve the single-attempt telemetry design. Do not re-add connection probes,
  retries, or backoff: each failed connection can cost five seconds awake.
- Follow existing C++ style and `Config::`, `Hardware::`, and `Timing::`
  constants. Keep telemetry names snake_case with units where applicable.

## Commands and verification

Run commands from the repository root.

| Task | Command |
| --- | --- |
| Build | `pio run` |
| Flash, after coordination | `pio run --target upload` |
| Follow serial | `./monitor` |
| Render previews | `scripts/preview` |
| Regenerate font header | `python3 scripts/generate_fonts.py` |

PlatformIO environment: `esp32-s3-supermini`, using generic
`esp32-s3-devkitc-1`, 4 MB flash, no PSRAM initialization, native USB CDC on
boot. Dependency versions are pinned in `platformio.ini`.

Previews need a C++ compiler, Python Pillow, and the ArduinoJson/Adafruit GFX
dependencies installed by `pio run`. They execute the firmware's dashboard
code and GFX rasterizer. Inspect affected normal, backup, flow-direction,
zero-flow, large-power, missing-data, and weather cases. Output is in
`previews/`, at native 296 × 128 and nearest-neighbor enlarged sizes.

`test/`, `tools/`, and `previews/` are gitignored local directories; do not
assume they exist in a fresh checkout. If the existing host checks are available:

```sh
c++ -std=c++11 -Wall -Wextra -pedantic -Iinclude \
  -I.pio/libdeps/esp32-s3-supermini/ArduinoJson/src \
  test/test_readings.cpp -o /tmp/solareink-test-readings
/tmp/solareink-test-readings
c++ -std=c++11 -Wall -Wextra -pedantic -Iinclude \
  test/test_logic.cpp -o /tmp/solareink-test-logic
/tmp/solareink-test-logic
```

For firmware changes, build and run applicable available host checks. For
layout/icons/fonts, also render and inspect previews; regenerate fonts after
glyph edits. Documentation-only changes need no firmware tests or flash.
Report unavailable checks and hardware verification honestly. After a
coordinated flash, verify the wake/cycle without resets or panics and the
panel refresh. If telemetry is enabled, check a subsequent `Influx: HTTP 204`
and the stored fields. Leave no test points behind.

## Source map

| Files | Responsibility |
| --- | --- |
| `src/main.cpp` | Cycle orchestration, completion targets, sleep scheduling |
| `src/network.cpp`, `include/readings.h` | Wi-Fi/HTTP polling; JSON parsing and validation |
| `src/clock_sync.cpp`, `include/logic.h` | NTP/timezone; scheduling, formatting, battery curve, weather groups |
| `src/battery.cpp` | Device battery ADC sampling |
| `src/dashboard.cpp`, `src/icons.cpp`, `src/display.cpp` | Layout, icons/arrows, panel lifecycle/light sleep |
| `include/metrics.h`, `src/metrics.cpp` | RTC record, emitted schema, Influx posting |
| `include/app_config.h` | Hardware/timing constants and firmware version |
| `include/config.example.h`, `include/solar_config.h` | Public template and ignored real settings |
| `scripts/monitor.py`, `monitor` | USB follower and PlatformIO Python wrapper |
| `scripts/render_preview.cpp`, `scripts/host/`, `scripts/preview` | Host rendering harness |
| `scripts/generate_fonts.py`, `include/fonts/dashboard_fonts.h` | Source glyphs and generated GFX fonts |
| `cad/`, `img/`, `README.md` | Case models, illustrations, owner-maintained build guide |

## Hardware and USB

The display's SDA/SCL labels mean SPI MOSI/SCK, not I²C.

| Connection | ESP32-S3 connection |
| --- | --- |
| SDA / MOSI | GPIO6 |
| SCL / SCK | GPIO1 |
| CS | GPIO5 |
| D/C | GPIO2 |
| RES | GPIO4 |
| BUSY | GPIO3 |
| GND | Common GND |
| VCC | Existing display supply; module accepts 3.3–5 V |
| Battery ADC | GPIO8 |

Do not assume a battery-powered board supplies regulated 5 V. GPIO signals
are 3.3 V. Keep driver `GxEPD2_290_C90c` and landscape rotation 1 (3 if upside
down) unless the hardware changes.

Battery divider: `LiPo+ → 220 kΩ → GPIO8 → 220 kΩ → GND`, with 100 nF from
GPIO8 to GND, parallel to the lower resistor. Divider factor: 2. Sample before
Wi-Fi/display initialization: 60 ms settling, 32 calibrated millivolt samples,
four highest and four lowest discarded. Correct against a meter with
`Hardware::BATTERY_CALIBRATION`. Device percentage is voltage-derived and
separate from Fronius storage SOC. Voltage trends do not establish measured
current, capacity, or runtime; keep estimates distinct from measurements.

Use PlatformIO's upload command. Known working local bootloader and firmware
headers are DIO, although `platformio.ini` currently says
`board_build.flash_mode = qio`. Verify generated headers/upload behavior before
changing flash settings. Manually forcing `--flash_mode qio` previously caused
`rst:0x7 TG0WDT_SYS_RST` watchdog reset loops.

Download mode: hold BOOT, tap RST, release BOOT. After upload, tap RST with
BOOT released if necessary. For manual esptool fallback, use PlatformIO's
Python environment. Known working parameters: `--before no_reset`,
`--flash_mode dio --flash_freq 80m --flash_size 4MB`; offsets: `0x0`
bootloader, `0x8000` partitions, `0xe000` boot_app0, `0x10000` firmware.
Verify actual artifacts before using manual offsets.

Prefer `./monitor`: it keeps DTR/RTS asserted and follows the USB serial
number across sleep/re-enumeration. Close competing monitors first. Clearing
DTR/RTS individually can reset the chip and lose RTC state. `SERIAL_WAIT_MS`
defaults to zero; increasing it costs awake time and does not guarantee early
boot logs are captured.

Changes to `RTC_DATA_ATTR` declarations or the staged `Metrics::Cycle` layout
can invalidate retained bytes. Preserve cold-boot initialization and
`Metrics::resetPending()`; never rely on RTC data surviving a reflash/reset.
Keep `Metrics::Cycle` trivially copyable, with no heap-owned data or pointers
whose targets cannot survive deep sleep.

## Configuration and scheduling

Copy `include/config.example.h` to `include/solar_config.h` for real settings.
`app_config.h` falls back to the example if the private file is absent, so a
successful build does not prove credentials were configured. Wi-Fi, location,
schedule, and Influx settings belong in `solar_config.h`; pins, calibration,
timezone, and NTP/API timing belong in `app_config.h`. The example includes a
placeholder Influx URL: explicitly set `INFLUX_URL` empty to disable telemetry.
Use 2.4 GHz Wi-Fi with access to the inverter.

- Default interval: 900 seconds, aligned to completion boundaries, not a raw
  sleep duration. It must be a positive multiple of 60.
- Default window: 06:00–23:00 local inclusive (69 quarter-hour refreshes).
  The 05:00 checkup measures the battery, forces NTP when connected, and posts
  pending telemetry without API fetches or screen refresh; then sleeps to
  about 05:59 for the 06:00 cycle.
- Timezone: `CET-1CEST,M3.5.0,M10.5.0/3`. System time is epoch time. Keep
  `tm_isdst = -1` before `mktime()` for local targets so sleeps account for DST.
- Wake early using retained preparation/refresh estimates. Display the intended
  completion minute. Up to two minutes late is accepted by default; later or
  out-of-window targets are skipped. A cold boot sleeps to its first aligned
  target. Without valid time, show `--:--` and use relative timing.
- Turn Wi-Fi off before alignment waits and display initialization/refresh.
  Hibernate the panel and end SPI afterward. Full refresh is about 27 seconds;
  there is no supported fast clock-only partial refresh.
- Without a serial host, light-sleep during alignment and in 40 ms panel BUSY
  slices. With `Serial` connected, stay awake to preserve USB. Power measurements
  with a monitor attached differ from untethered operation.

## Data and rendering invariants

Fronius endpoint: `/solar_api/v1/GetPowerFlowRealtimeData.fcgi`. Preserve the
installation-specific signs:

| Value | Meaning |
| --- | --- |
| `P_PV` | Positive solar production |
| `P_Load` | Negative house consumption; display absolute magnitude |
| `P_Grid` | Positive import, negative export |
| `P_Akku` | Positive discharge, negative charge |
| `Body.Data.Inverters["1"].SOC` | House battery percentage |
| `BackupMode` | Boolean or integer 0/1; distinguish unknown from inactive |

Open-Meteo fetches today's daily min/max temperature, maximum precipitation
probability, and WMO code over HTTP with Celsius and `timezone=auto`.
These are daily forecast values, not current conditions.

- Preserve `NAN` and known-state validation in `include/readings.h`. Missing,
  malformed, out-of-range, and API-error data must not become zero or stale
  data shown as fresh. Independently valid fields can still be displayed.
- HTTP/1.0 requests have bounded connect/read/body timeouts and response size;
  preserve those bounds. Each API gets ten attempts with 250 ms pauses;
  weather still runs after Fronius fails. Outages can extend awake time.
- Cached Wi-Fi BSSID/channel gets a six-second attempt. On failure, clear it
  and fall back to up to four 15-second scan attempts with 500 ms pauses,
  preferring the strongest matching AP. Disconnected status alone does not
  establish a wrong password.
- NTP runs on first connection, every six hours, and at the checkup: up to
  four eight-second attempts with 500 ms pauses. Stop background SNTP before
  radio shutdown; RTC system time continues through sleep.
- Dashboard text is German. Preserve red absolute house consumption, grid and
  storage direction arrows, and the white-on-red `NOTSTROM AKTIV` backup banner
  with open-switch symbol. Missing values show `--`; zero flows have no arrowhead.
  Grid import/storage discharge point toward the house; export/charge point
  away. Storage power uses a red up arrow for discharge, black down for charge.
- Use W/kW with smaller units and fitting precision/fonts. Preserve shared
  connection centerlines and symmetric/mirrored icon and arrow geometry.
- Fonts use original 5 × 7 pixel glyphs enlarged by whole-number multiples.
  Edit the generator and regenerate the header; do not substitute downscaled
  or stretched outline fonts. Keep icons at native pixel resolution.

## Telemetry contract

`src/metrics.cpp` is the authoritative emitted schema; `include/metrics.h`
defines the RTC record. Measurement: `solar`. Exactly three tags: `name`
(`DEVICE_NAME`), `mac`, `fw`. Everything else, including BSSID/IP, stays a field.
Do not add derived/constant fields, credentials, URLs, coordinates, NTP server
names, forecast date/timezone, or core version.

Stage the completed cycle before sleep. Post it during the next connected
cycle after API fetching (after NTP at the checkup). Resets clear staging.
There is one slot and one HTTP/1.0 POST to `/api/v2/write`, second precision,
five-second connect/read timeouts. A failed POST consumes the record; the next
cycle offers a new record, not a retry of the failed point. A Wi-Fi-less cycle
also replaces the pending record when staging its own data.

Interpretation details that are easy to miss:

- Point timestamp: cycle wake epoch seconds, calculated at staging from current
  time minus awake duration. Without valid time at staging, omit the timestamp
  and use Influx server time.
- `plan`: 0 refresh, 1 no target/scheduled sleep, 2 too late, 3 checkup.
  `target` is epoch seconds. Despite its name, `missed_ms` is the missed target's
  epoch milliseconds, not a lateness duration. `completion_err_ms` is a signed
  completion offset.
- `prep_ms` measures `Screen::prepare()` only. The scheduler's retained estimate
  also includes startup, ADC, Wi-Fi, and polling. `awake_ms` includes light sleep
  and is captured before staging/final serial flush.
- `usb` is `bool(Serial)`, not power-cable presence. `chip_temp_c` is chip
  temperature, not ambient. Heap fields are bytes.
- `prev_post_ok`: -1 unknown, 0 failure, 1 success. Together with `prev_post_ms`,
  it describes the earlier post attempt, not the carrying point's HTTP result.
- Missing numeric values are omitted. Wi-Fi details require a connection;
  API fields are absent on checkups/Wi-Fi failures; panel timing requires a
  refresh. First-sync `ntp_drift_ms` can be huge after cold boot.
- `device_v` and voltage-derived `device_pct` do not measure current/capacity.
