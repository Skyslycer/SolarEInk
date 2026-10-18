# SolarEInk — WeAct 2.9-inch solar dashboard

ESP32-S3 SuperMini firmware for the **296 × 128 black/white/red WeAct display**.
It reads the local Fronius Solar API and today's Open-Meteo forecast, measures
its own LiPo, renders the dashboard, hibernates the panel, and deep-sleeps.

## Wiring

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
GPIO8 to GND**. Measurement uses 32 calibrated millivolt samples before Wi-Fi or
display initialization, a 60ms settling delay, and a trimmed mean. The divider
factor is 2. The small top-right percentage is an approximate rested LiPo
voltage-to-SOC curve; charging, load, temperature, resistor tolerance, and ADC
error affect it. `BATTERY_CALIBRATION` can correct the voltage against a meter.
This percentage is separate from the **Fronius storage SOC** at bottom right.

The driver is `GxEPD2_290_C90c` (GDEM029C90 / SSD1680), matching
[WeAct's three-color example](https://github.com/WeActStudio/WeActStudio.EpaperModule/blob/master/Example/EpaperModuleTest_Arduino_ESP32/EpaperModuleTest_Arduino_ESP32.ino).
Landscape rotation is 1; set `Hardware::DISPLAY_ROTATION` to 3 if upside down.

## Configure, build, upload, monitor

The existing local Wi-Fi and location settings are in `include/solar_config.h`,
which is ignored by Git. For a fresh checkout copy `include/config.example.h`
to that path and fill in the SSID, password, inverter IP, and coordinates.
`REFRESH_SECONDS` defaults to **900** and means the target screen-update
interval, aligned to quarter-hour wall-clock marks such as 13:00, 13:15, and
13:30. It does not mean that the board sleeps for that entire duration. It
must be a multiple of 60. `FIRST_UPDATE_HOUR` (default **6**) and
`LAST_UPDATE_HOUR` (default **23**) bound the daily update window in local
time: the last update is the 23:00 mark, and the board sleeps through the
night until the 06:00 mark. The overnight sleep is split by a time-sync-only
checkup at `CHECKUP_HOUR` (default **5**, local time); afterwards the board
sleeps until `MORNING_WAKE_BEFORE_MINUTES` (default **1**) before the first
update. Pin, ADC, timezone, and NTP settings are in `include/app_config.h`.

```sh
pio run
pio run --target upload
./monitor
```

Use a 2.4GHz Wi-Fi network that can reach the inverter. The firmware targets
4MB flash with no PSRAM initialization. If upload cannot catch the short awake
window, enter BOOT mode manually: hold BOOT, press/release RESET, release BOOT.
If it remains in download mode after flashing, press RESET with BOOT released.

Wi-Fi gets one initial connection attempt plus three retries, each with a
15-second timeout and a 500ms pause between attempts. API requests have their
own ten-attempt retry limit.

`./monitor` follows this board's USB serial number across deep-sleep disconnects
and port-number changes. Close other monitors first; Ctrl+C exits. It uses the
PlatformIO Python environment under `${PLATFORMIO_CORE_DIR:-$HOME/.platformio}`.
For another board, pass `--serial-number` with the value from `pio device list`.
`pio device monitor` is restricted to `/dev/ttyACM*` but can still miss reconnects.
Keep DTR and RTS asserted for native USB Serial/JTAG: clearing them individually
caused USB resets and lost RTC boot counts on this board. Early output can still
be missed when the host attaches late; `SERIAL_WAIT_MS` optionally adds grace
at the cost of awake time.

## Display behavior

- Top left: today's min/max °C, maximum precipitation probability, and an icon
  selected from all documented Open-Meteo WMO codes (sun, cloud, fog, drizzle,
  rain, freezing precipitation, snow, showers, thunderstorms, hail).
- Top center: solar production and the sun/panel icon.
- Top right: Wi-Fi strength/offline indicator, the device LiPo percentage, and
  local `HH:MM`. Timezone defaults to Europe/Berlin's CET/CEST rules with DST.
- Middle: house consumption, always red and displayed as an absolute value.
- Bottom left: grid magnitude and arrow toward the house for import, away for
  export. Backup mode replaces this entire section with white **NOTSTROM AKTIV**
  on red and an **open-switch** symbol toward the house.
- Bottom right: Fronius battery SOC and four dynamic red bars. Discharge points
  toward the house and has a red **up** arrow beside power. Charge points away
  from the house and has a black **down** arrow beside power.
- All power fields automatically switch from W to kW. Units are smaller than
  the digits; kW precision drops to one decimal where space is tight, followed
  by a smaller numeric font if needed. Missing or invalid data shows `--`; it is never silently treated as
  zero or retained as though fresh. Zero flows have no directional arrowhead.

Icons are drawn at native pixel resolution. Grid and battery connections share
one horizontal centerline and mirrored arrowheads; the solar and battery power
arrows use the same shape vertically. The pylon, panel cells, Wi-Fi arcs, and
battery outlines have symmetric geometry. Wi-Fi aligns with the status row.

Open-Meteo requests only one day's four daily fields, with `timezone=auto` and
Celsius. Transport remains plain HTTP for this public forecast. Fronius is
queried at `/solar_api/v1/GetPowerFlowRealtimeData.fcgi` with the previously
verified installation-specific signs. Neither endpoint failure prevents the
other request or the device returning to sleep.

The firmware calls standard `WiFi.begin()`. A successful cycle caches its
access point's BSSID and channel in RTC memory, and the next connect targets
that AP directly with a bounded attempt (six seconds), skipping the scan. On
failure the cache is cleared and the driver scans all channels, preferring
the strongest AP when several share the SSID. Each API
request gets up to ten attempts, with a 250ms pause between failures. Long
outages can extend awake time considerably; weather polling still runs after
the Fronius retries finish.

On Wi-Fi failure, Serial reports the disconnect reason and whether association
ever succeeded. It does not print the password. A disconnected status alone
does not identify a bad password.

## Refresh timing and power

This three-color panel's full update is approximately **27 seconds**, as given
by the [driver](https://github.com/ZinggJM/GxEPD2/blob/master/src/epd3c/GxEPD2_290_C90c.h).
Its partial-update API supports writing a region of controller memory, but
still performs a full-screen waveform. Both full and partial paths specify
27 seconds, so several partial updates followed by a full update would not
avoid the flashing or save the refresh time. It does **not** have a supported
fast clock-only partial refresh. Visible flashing during that waveform
is expected. Earlier battery estimates for the display-free tester do not
include this extra awake/refresh energy.

NTP sync runs on first connection and every six hours thereafter. Each sync
allows four attempts (one plus three retries), each with an eight-second
timeout and a 500ms pause. The ESP32's
RTC-backed system clock keeps time through deep sleep. Each screen is labelled
with its intended **completion** minute. The next wake is scheduled early
enough for ADC, Wi-Fi, polling, and refresh; actual preparation and refresh
lengths are retained in RTC memory to adjust the next cycle. Wi-Fi turns off
before any alignment wait or screen update. When no USB host is attached, the
CPU light-sleeps through the alignment wait before the refresh and, in 40ms
slices via GxEPD2's busy callback, through the 27-second waveform (about
22mA to 1mA for those phases); with a monitor attached it stays awake so the
bench behaves exactly as before. Once finished, the display is explicitly
hibernated before the ESP32 enters deep sleep.

This aims for completion at the full minute, not a zero-latency clock: panel
temperature, network delays, and RTC drift affect accuracy. Serial reports the
actual completion error. A completion up to `TARGET_LATE_TOLERANCE_MS` (two
minutes by default) after a boundary still refreshes with that boundary's
minute. Anything later skips the interval: the device deep-sleeps through to
the next one instead of idling awake. Targets outside the daily update window
(first 06:00, last 23:00) are skipped the same way. The quiet hours run as
two sleeps: at 05:00 the board wakes for a Wi-Fi/NTP-only checkup (no fetch,
no screen update) to correct the internal RC clock's overnight drift, then
sleeps to 05:55 and runs the normal cycle for the 06:00 update, keeping the
first update of the day punctual. A cold boot has no carried target yet and
likewise sleeps through to its first aligned update. If no time has ever
synchronized, the clock shows `--:--` and updates use relative timing until
NTP succeeds.

Serial still includes RTC boot/wake information, ADC voltage, per-request
status/timing, parsed values, refresh time, target error, awake time and sleep
duration. Check these when measuring externally; neither firmware timestamps
nor the ADC measure supply current.

## Code and verification

- `src/main.cpp`: wake → ADC → network → render → minute scheduling → sleep.
- `src/network.cpp`: Wi-Fi, bounded HTTP requests, API parsing diagnostics.
- `src/battery.cpp` and `src/clock_sync.cpp`: LiPo ADC and NTP/RTC time.
- `src/dashboard.cpp`: layout and text; `src/icons.cpp`: icons and flow arrows;
  `src/display.cpp`: panel driver.
- `include/logic.h`: formatting, voltage curve, weather groups, time boundaries.

The desktop previews execute **the same dashboard drawing code and Adafruit
GFX rasterizer** used on the ESP32. No hardware is needed:

```sh
scripts/preview
```

Requires a C++ compiler and Python Pillow. PNGs appear in `previews/`, including
normal, backup, all four grid/battery flow combinations, zero flow, large kW
values, unavailable data, and every
supported weather code. Native previews are exactly 296 × 128 pixels; `-2x`/`-4x`
versions enlarge each pixel without smoothing. These are raster previews of
the firmware, not an approximation drawn in a separate graphics program.
`previews/flow-directions.png` puts the four flow combinations together for review.

Text uses original 5 × 7 pixel glyphs, enlarged only by whole-number multiples
in both dimensions. This keeps strokes consistent and symmetric glyphs
symmetric; there is no downscaled or stretched outline font. Regenerate the
bundled header with `python3 scripts/generate_fonts.py` after editing glyphs.

After PlatformIO has installed ArduinoJson, run the host checks:

```sh
c++ -std=c++11 -Wall -Wextra -pedantic -Iinclude \
  -I.pio/libdeps/esp32-s3-supermini/ArduinoJson/src \
  test/test_readings.cpp -o /tmp/solareink-test-readings
/tmp/solareink-test-readings
c++ -std=c++11 -Wall -Wextra -pedantic -Iinclude \
  test/test_logic.cpp -o /tmp/solareink-test-logic
/tmp/solareink-test-logic
```
