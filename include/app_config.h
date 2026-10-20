#pragma once

#if __has_include("solar_config.h")
#include "solar_config.h"
#else
#include "config.example.h"
#endif

namespace App {
// Reported as the Influx `fw` tag. Bump on every flashed change.
constexpr char FIRMWARE_VERSION[] = "1.0.0";
}

namespace Hardware {
constexpr int EPD_MOSI = 6;
constexpr int EPD_SCK = 1;
constexpr int EPD_CS = 5;
constexpr int EPD_DC = 2;
constexpr int EPD_RST = 4;
constexpr int EPD_BUSY = 3;
constexpr int BATTERY_ADC = 8;
constexpr float BATTERY_DIVIDER = 2.0f;  // 220k / 220k
constexpr float BATTERY_CALIBRATION = 1.0f;  // Correct against a multimeter if needed
constexpr int BATTERY_SAMPLES = 32;
constexpr int DISPLAY_ROTATION = 1;  // Landscape 296 x 128; use 3 to turn upside down
}

namespace Timing {
constexpr char TIMEZONE[] = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr char NTP_SERVER[] = "pool.ntp.org";
constexpr uint32_t NTP_TIMEOUT_MS = 8000;
constexpr uint32_t NTP_RESYNC_SECONDS = 6 * 3600;
constexpr unsigned NTP_RETRIES = 3;
constexpr uint32_t NTP_RETRY_DELAY_MS = 500;
constexpr unsigned WIFI_RETRIES = 3;
constexpr uint32_t WIFI_RETRY_DELAY_MS = 500;
// Bounded first attempt on the access point cached by the previous successful
// cycle, which skips the full-channel scan. On failure the cache is cleared
// and the normal scan + retry path below runs.
constexpr uint32_t WIFI_CACHED_TIMEOUT_MS = 6000;
constexpr unsigned API_ATTEMPTS = 10;
constexpr uint32_t API_RETRY_DELAY_MS = 250;
// Telemetry POST: one bounded attempt inside the Wi-Fi window the cycle already
// has open. Timeouts match the API fetches so a slow first packet after Wi-Fi
// power save still gets through; the next cycle tries again on failure.
constexpr uint16_t INFLUX_CONNECT_TIMEOUT_MS = 5000;
constexpr uint16_t INFLUX_READ_TIMEOUT_MS = 5000;
constexpr uint32_t INITIAL_REFRESH_MS = 27000;
constexpr uint32_t INITIAL_PREPARE_MS = 3000;
constexpr uint32_t WAKE_MARGIN_MS = 1000;
// CPU light-sleep slice while the panel busy-waits through its ~27 s waveform
// (only when no USB host is attached). Also bounds how late a finished
// refresh is noticed, so it stays well under the completion tolerance.
constexpr uint32_t PANEL_BUSY_SLEEP_SLICE_US = 40000;
// Accept a completion up to this late and keep displaying the target's minute.
// Anything later skips the interval and deep-sleeps through to the next one.
constexpr uint32_t TARGET_LATE_TOLERANCE_MS = 120000;
}

static_assert(Config::REFRESH_SECONDS >= 60 && Config::REFRESH_SECONDS % 60 == 0,
              "Refresh interval must be a positive whole number of minutes");
static_assert(Config::FIRST_UPDATE_HOUR >= 0 && Config::FIRST_UPDATE_HOUR <= Config::LAST_UPDATE_HOUR,
              "Update window hours must be ordered within one day");
static_assert(Config::LAST_UPDATE_HOUR <= 23, "Update window hours must be within one day");
static_assert(Config::CHECKUP_HOUR >= 0 && Config::CHECKUP_HOUR <= 23,
              "Checkup hour must be within one day");
static_assert(Config::MORNING_WAKE_BEFORE_MINUTES >= 0 && Config::MORNING_WAKE_BEFORE_MINUTES < 60,
              "Morning wake margin must be under an hour");
static_assert(Config::WEATHER_LATITUDE >= -90 && Config::WEATHER_LATITUDE <= 90,
              "Invalid weather latitude");
static_assert(Config::WEATHER_LONGITUDE >= -180 && Config::WEATHER_LONGITUDE <= 180,
              "Invalid weather longitude");
