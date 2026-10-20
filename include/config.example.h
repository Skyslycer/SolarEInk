#pragma once

#include <stdint.h>

namespace Config {
constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr char FRONIUS_IP[] = "192.168.1.100";  // IP only, no http:// or path
constexpr char DEVICE_NAME[] = "solareink";  // Influx tag `name`
constexpr double WEATHER_LATITUDE = 52.5200;  // Replace with your location
constexpr double WEATHER_LONGITUDE = 13.4050;
constexpr uint32_t REFRESH_SECONDS = 900;
constexpr int FIRST_UPDATE_HOUR = 6;   // First update of the day, at :00 local time
constexpr int LAST_UPDATE_HOUR = 23;   // Last update of the day, at :00 local time
constexpr int CHECKUP_HOUR = 5;             // Night time-sync wake, at :00 local time
constexpr int MORNING_WAKE_BEFORE_MINUTES = 1;  // Wake this early for the first update

constexpr uint32_t WIFI_TIMEOUT_MS = 15000;
constexpr uint16_t HTTP_CONNECT_TIMEOUT_MS = 5000;
constexpr uint16_t HTTP_READ_TIMEOUT_MS = 8000;
constexpr uint32_t HTTP_BODY_TIMEOUT_MS = 8000;
constexpr unsigned MAX_RESPONSE_BYTES = 8192;
// Optional USB attach grace period. Keep zero for power measurements.
constexpr uint32_t SERIAL_WAIT_MS = 0;
constexpr uint32_t SERIAL_TX_TIMEOUT_MS = 50;

// Optional InfluxDB v2 telemetry. Leave INFLUX_URL empty to disable logging.
// One record per cycle is posted on the next cycle's Wi-Fi window; tags are
// DEVICE_NAME, the Wi-Fi MAC, and the firmware version.
constexpr char INFLUX_URL[] = "http://192.168.1.50:8086";  // server base URL, no path
constexpr char INFLUX_ORG[] = "home";                      // organization name or ID
constexpr char INFLUX_BUCKET[] = "solareink";
constexpr char INFLUX_TOKEN[] = "YOUR_INFLUX_TOKEN";
}  // namespace Config
