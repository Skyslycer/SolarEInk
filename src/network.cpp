#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <atomic>
#include "app_config.h"
#include "network.h"

namespace Network {
namespace {
std::atomic<unsigned> disconnectReason{0};
std::atomic<bool> associated{false};

WifiStats wifiStats;
FetchStats froniusFetch;
FetchStats weatherFetch;

// Access point cached by the last successful connect. RTC memory survives
// deep sleep; the channel range check rejects stale/garbage values after any
// reset that clears RTC memory, and failures fall back to the scan path.
RTC_DATA_ATTR int cachedChannel = 0;
RTC_DATA_ATTR uint8_t cachedBssid[6] = {0};

void wifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        disconnectReason.store(info.wifi_sta_disconnected.reason);
        const auto& d = info.wifi_sta_disconnected;
        Serial.printf("Wi-Fi disconnect: reason=%u, AP=%02X:%02X:%02X:%02X:%02X:%02X\n",
                      d.reason, d.bssid[0], d.bssid[1], d.bssid[2],
                      d.bssid[3], d.bssid[4], d.bssid[5]);
    }
    if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) associated.store(true);
}

const char* reasonName(unsigned reason) {
    switch (reason) {
        case 0: return "no disconnect event";
        case WIFI_REASON_AUTH_EXPIRE: return "authentication timeout";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4-way handshake timeout";
        case WIFI_REASON_BEACON_TIMEOUT: return "router beacon timeout";
        case WIFI_REASON_NO_AP_FOUND: return "configured access point not found";
        case WIFI_REASON_AUTH_FAIL: return "authentication rejected";
        case WIFI_REASON_ASSOC_FAIL: return "association failed";
        case WIFI_REASON_HANDSHAKE_TIMEOUT: return "security handshake timeout";
        default: return "other driver/router reason";
    }
}

}

bool connectWifi() {
    const uint32_t start = millis();
    wifiStats = WifiStats{};
    if (strcmp(Config::WIFI_SSID, "YOUR_WIFI_SSID") == 0) {
        Serial.println("Wi-Fi: configure include/solar_config.h first (skipped, 0 ms)");
        wifiStats.ms = millis() - start;
        return false;
    }
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    // The scan settings below apply to the fallback path: a successful cycle
    // caches its access point, and the next connect targets it directly
    // without scanning. When that fails, compare same-SSID access points by
    // signal instead of stopping at the first match.
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
    disconnectReason.store(0);
    associated.store(false);
    const auto eventId = WiFi.onEvent(wifiEvent);
    if (!WiFi.mode(WIFI_STA)) {
        WiFi.removeEvent(eventId);
        Serial.println("Wi-Fi: station initialization failed");
        wifiStats.ms = millis() - start;
        wifiStats.status = WiFi.status();
        return false;
    }
    wl_status_t beginStatus = WL_IDLE_STATUS;
    if (cachedChannel >= 1 && cachedChannel <= 14) {
        wifiStats.cachedUsed = true;
        ++wifiStats.attempts;
        Serial.printf("Wi-Fi: cached AP %02X:%02X:%02X:%02X:%02X:%02X channel %d\n",
                      cachedBssid[0], cachedBssid[1], cachedBssid[2],
                      cachedBssid[3], cachedBssid[4], cachedBssid[5], cachedChannel);
        const uint32_t attemptStart = millis();
        beginStatus = WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD, cachedChannel, cachedBssid);
        while (WiFi.status() != WL_CONNECTED && millis() - attemptStart < Timing::WIFI_CACHED_TIMEOUT_MS)
            delay(20);
        if (WiFi.status() != WL_CONNECTED) {
            wifiStats.cachedFailed = true;
            Serial.printf("Wi-Fi: cached AP failed, status=%d, elapsed=%lu ms, reason=%u (%s)\n",
                          static_cast<int>(WiFi.status()),
                          static_cast<unsigned long>(millis() - attemptStart),
                          disconnectReason.load(), reasonName(disconnectReason.load()));
            Serial.println("Wi-Fi: falling back to full-channel scan");
            cachedChannel = 0;
            WiFi.disconnect(false, false);
            disconnectReason.store(0);
            associated.store(false);
        }
    }
    for (unsigned attempt = 0;
         attempt <= Timing::WIFI_RETRIES && WiFi.status() != WL_CONNECTED;
         ++attempt) {
        if (attempt > 0) {
            Serial.printf("Wi-Fi: retry %u/%u in %lu ms\n", attempt,
                          Timing::WIFI_RETRIES,
                          static_cast<unsigned long>(Timing::WIFI_RETRY_DELAY_MS));
            WiFi.disconnect(false, false);
            delay(Timing::WIFI_RETRY_DELAY_MS);
            disconnectReason.store(0);
            associated.store(false);
        }
        ++wifiStats.attempts;
        const uint32_t attemptStart = millis();
        beginStatus = WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
        while (WiFi.status() != WL_CONNECTED && millis() - attemptStart < Config::WIFI_TIMEOUT_MS)
            delay(20);
        if (WiFi.status() == WL_CONNECTED) break;

        Serial.printf("Wi-Fi: attempt %u/%u failed, status=%d, elapsed=%lu ms, reason=%u (%s)\n",
                      attempt + 1, Timing::WIFI_RETRIES + 1,
                      static_cast<int>(WiFi.status()),
                      static_cast<unsigned long>(millis() - attemptStart),
                      disconnectReason.load(), reasonName(disconnectReason.load()));
    }
    WiFi.removeEvent(eventId);
    if (WiFi.status() != WL_CONNECTED) {
        wifiStats.ms = millis() - start;
        wifiStats.status = static_cast<int>(WiFi.status());
        wifiStats.reason = static_cast<int>(disconnectReason.load());
        Serial.printf("Wi-Fi: FAILED, status=%d, duration=%lu ms\n",
                      static_cast<int>(WiFi.status()), static_cast<unsigned long>(millis() - start));
        const unsigned reason = disconnectReason.load();
        Serial.printf("Wi-Fi detail: begin status=%d, associated=%s, reason=%u (%s)\n",
                      static_cast<int>(beginStatus), associated.load() ? "yes" : "no",
                      reason, reasonName(reason));
        return false;
    }
    const int32_t channel = WiFi.channel();
    uint8_t* bssid = WiFi.BSSID();
    if (channel >= 1 && channel <= 14 && bssid) {
        cachedChannel = channel;
        memcpy(cachedBssid, bssid, sizeof(cachedBssid));
    }
    wifiStats.ms = millis() - start;
    wifiStats.status = static_cast<int>(WiFi.status());
    wifiStats.reason = static_cast<int>(disconnectReason.load());
    Serial.printf("Wi-Fi: connected, IP=%s, RSSI=%d dBm, duration=%lu ms\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                  static_cast<unsigned long>(millis() - start));
    return true;
}

// HTTP/1.0 requests avoid chunked transfer framing; the full body is bounded by
// both size and elapsed time, then parsed so truncated replies are rejected.
bool fetchJsonOnce(const char* label, const String& url, JsonDocument& document, FetchStats& stats) {
    const uint32_t start = millis();
    WiFiClient client;
    HTTPClient http;
    http.useHTTP10(true);
    http.setReuse(false);
    http.setConnectTimeout(Config::HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(Config::HTTP_READ_TIMEOUT_MS);
    const char* headers[] = {"Transfer-Encoding"};
    http.collectHeaders(headers, 1);
    bool success = false;
    size_t bytes = 0;
    do {
        if (!http.begin(client, url)) {
            stats.http = -1;
            Serial.printf("%s: cannot initialize HTTP\n", label);
            break;
        }
        const int status = http.GET();
        stats.http = status;
        Serial.printf("%s: HTTP %d, headers after %lu ms\n", label, status,
                      static_cast<unsigned long>(millis() - start));
        if (status != HTTP_CODE_OK) {
            if (status < 0) Serial.printf("%s: %s\n", label, HTTPClient::errorToString(status).c_str());
            break;
        }
        if (http.header("Transfer-Encoding").length() != 0) {
            Serial.printf("%s: unexpected transfer encoding\n", label);
            break;
        }
        const int expected = http.getSize();
        if (expected > static_cast<int>(Config::MAX_RESPONSE_BYTES)) {
            Serial.printf("%s: response exceeds size limit\n", label);
            break;
        }
        String body;
        if (!body.reserve(expected > 0 ? expected : 1024)) {
            Serial.printf("%s: cannot allocate response buffer\n", label);
            break;
        }
        WiFiClient& stream = http.getStream();
        const uint32_t bodyStart = millis();
        bool bodyOk = true;
        while (expected < 0 || body.length() < static_cast<unsigned>(expected)) {
            if (millis() - bodyStart >= Config::HTTP_BODY_TIMEOUT_MS) {
                Serial.printf("%s: response body timeout\n", label);
                bodyOk = false;
                break;
            }
            const int available = stream.available();
            if (available > 0) {
                char buffer[256];
                size_t count = min(static_cast<size_t>(available), sizeof(buffer));
                if (expected >= 0) count = min(count, static_cast<size_t>(expected) - body.length());
                const int received = stream.read(reinterpret_cast<uint8_t*>(buffer), count);
                if (received > 0) {
                    if (body.length() + received > Config::MAX_RESPONSE_BYTES ||
                        !body.concat(buffer, received)) {
                        Serial.printf("%s: response buffer limit/allocation failure\n", label);
                        bodyOk = false;
                        break;
                    }
                }
            } else if (!stream.connected()) {
                break;
            } else {
                delay(1);
            }
        }
        bytes = body.length();
        stats.bytes = static_cast<uint32_t>(bytes);
        if (!bodyOk) break;
        if (expected >= 0 && bytes != static_cast<size_t>(expected)) {
            Serial.printf("%s: truncated response\n", label);
            break;
        }
        const DeserializationError error = deserializeJson(document, body);
        if (error) {
            Serial.printf("%s: JSON error: %s\n", label, error.c_str());
            break;
        }
        success = true;
    } while (false);
    http.end();
    Serial.printf("%s: request+body+JSON=%lu ms, bytes=%u, %s\n", label,
                  static_cast<unsigned long>(millis() - start), static_cast<unsigned>(bytes),
                  success ? "OK" : "FAILED");
    return success;
}

bool fetchJson(const char* label, const String& url, JsonDocument& document, FetchStats& stats) {
    stats = FetchStats{};
    const uint32_t start = millis();
    for (unsigned attempt = 1; attempt <= Timing::API_ATTEMPTS; ++attempt) {
        ++stats.attempts;
        document.clear();
        Serial.printf("%s: attempt %u/%u\n", label, attempt, Timing::API_ATTEMPTS);
        if (fetchJsonOnce(label, url, document, stats)) {
            stats.ms = millis() - start;
            return true;
        }
        if (attempt < Timing::API_ATTEMPTS) delay(Timing::API_RETRY_DELAY_MS);
    }
    stats.ms = millis() - start;
    Serial.printf("%s: failed after %u attempts\n", label, Timing::API_ATTEMPTS);
    return false;
}

void printValue(const char* label, double value, const char* unit) {
    if (std::isfinite(value)) Serial.printf("  %s: %.2f %s\n", label, value, unit);
    else Serial.printf("  %s: unavailable\n", label);
}

bool pollFronius(Readings::Fronius& reading) {
    JsonDocument document;
    const String url = String("http://") + Config::FRONIUS_IP +
                       "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";
    if (!fetchJson("Fronius", url, document, froniusFetch)) return false;
    reading = Readings::parseFronius(document.as<JsonVariantConst>());
    printValue("Solar production", reading.pv, "W");
    printValue("House consumption", reading.consumption(), "W");
    printValue("Grid", std::fabs(reading.grid), "W");
    Serial.printf("  Grid direction: %s\n", Readings::gridDirection(reading.grid));
    printValue("Battery", std::fabs(reading.battery), "W");
    Serial.printf("  Battery direction: %s\n", Readings::batteryDirection(reading.battery));
    printValue("SOC", reading.soc, "%");
    Serial.printf("  BackupMode: %s\n", !reading.backupKnown ? "unavailable" :
                  reading.backup ? "NOTSTROM AKTIV" : "inactive");
    if (!reading.complete()) Serial.println("Fronius: missing/invalid fields or API error");
    return reading.complete();
}

bool pollWeather(Readings::Weather& reading) {
    // Public forecast over HTTP: no TLS handshake or clock synchronization.
    // timezone=auto makes index 0 today at the configured coordinates.
    const String url = String("http://api.open-meteo.com/v1/forecast?latitude=") +
                       String(Config::WEATHER_LATITUDE, 6) + "&longitude=" +
                       String(Config::WEATHER_LONGITUDE, 6) +
                       "&daily=temperature_2m_min,temperature_2m_max,"
                       "precipitation_probability_max,weather_code"
                       "&temperature_unit=celsius&timezone=auto&forecast_days=1";
    JsonDocument document;
    if (!fetchJson("Open-Meteo", url, document, weatherFetch)) return false;
    reading = Readings::parseWeather(document.as<JsonVariantConst>());
    Serial.printf("  Forecast date: %s, timezone: %s\n",
                  document["daily"]["time"][0].as<const char*>() ?: "unavailable",
                  document["timezone"].as<const char*>() ?: "unavailable");
    printValue("Today minimum", reading.minimum, "C");
    printValue("Today maximum", reading.maximum, "C");
    printValue("Precipitation probability max", reading.precipitation, "%");
    printValue("weather_code", reading.code, "WMO");
    if (!reading.complete()) Serial.println("Open-Meteo: missing/invalid fields or API error");
    return reading.complete();
}

const WifiStats& lastWifiStats() { return wifiStats; }
const FetchStats& froniusStats() { return froniusFetch; }
const FetchStats& weatherStats() { return weatherFetch; }
}  // namespace Network
