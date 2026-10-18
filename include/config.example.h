#pragma once

#include <stdint.h>

namespace Config {
constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr char FRONIUS_IP[] = "192.168.1.100";  // IP only, no http:// or path
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
}  // namespace Config
