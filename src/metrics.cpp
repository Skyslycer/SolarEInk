#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include "app_config.h"
#include "clock_sync.h"
#include "metrics.h"

static_assert(std::is_trivially_copyable<Metrics::Cycle>::value,
              "Cycle must stay a plain struct: it is kept in RTC memory across deep sleep");

namespace {
using Metrics::Cycle;

// The record is completed after the panel refresh and posted on the next
// boot's Wi-Fi window. RTC memory carries it across the deep sleep in between.
// A raw byte image avoids any default-constructor question for RTC objects.
// setup() calls resetPending() on any non-deep-sleep boot, so a reflash or
// cold boot cannot resurrect stale bytes from an older firmware layout.
RTC_DATA_ATTR uint8_t pendingBytes[sizeof(Cycle)];
RTC_DATA_ATTR bool pendingValid = false;
RTC_DATA_ATTR int32_t lastPostOk = -1;
RTC_DATA_ATTR uint32_t lastPostMs = 0;

Cycle currentCycle;

void escapeTag(String& out, const char* value) {
    for (const char* p = value; *p; ++p) {
        if (*p == ',' || *p == '=' || *p == ' ' || *p == '\\') out += '\\';
        out += *p;
    }
}

void escapeFieldString(String& out, const char* value) {
    for (const char* p = value; *p; ++p) {
        if (*p == '"' || *p == '\\') out += '\\';
        out += *p;
    }
}

// Line-protocol field builder: a space before the first field, commas between
// the rest. Values that are not applicable are skipped entirely, so Influx sees
// a missing field instead of a misleading zero.
struct Line {
    String text;
    unsigned fields = 0;

    void intField(const char* key, int64_t value) {
        char buffer[24];
        snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
        text += fields++ ? ',' : ' ';
        text += key;
        text += '=';
        text += buffer;
        text += 'i';
    }

    void floatField(const char* key, double value, int precision) {
        if (!std::isfinite(value)) return;
        char buffer[24];
        snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
        text += fields++ ? ',' : ' ';
        text += key;
        text += '=';
        text += buffer;
    }

    void boolField(const char* key, bool value) {
        text += fields++ ? ',' : ' ';
        text += key;
        text += '=';
        text += value ? "true" : "false";
    }

    void stringField(const char* key, const char* value) {
        if (!value || !*value) return;
        text += fields++ ? ',' : ' ';
        text += key;
        text += '=';
        text += '"';
        escapeFieldString(text, value);
        text += '"';
    }
};

String buildLine(const Cycle& record, unsigned& fieldCount) {
    Line line;
    line.text.reserve(1400);
    line.text += "solar";
    line.text += ",name=";
    escapeTag(line.text, Config::DEVICE_NAME);
    line.text += ",mac=";
    line.text += WiFi.macAddress();
    line.text += ",fw=";
    escapeTag(line.text, App::FIRMWARE_VERSION);

    line.intField("boot_count", record.bootCount);
    line.intField("wake", record.wakeCause);
    line.intField("reset", record.resetReason);
    line.boolField("checkup", record.checkup);
    line.boolField("usb", record.usb);
    line.intField("plan", record.plan);
    if (record.targetEpoch > 0 && record.plan == Metrics::PlanRefresh)
        line.intField("target", record.targetEpoch);
    if (record.missedMs > 0) line.intField("missed_ms", record.missedMs);
    if (record.plan == Metrics::PlanRefresh && record.targetEpoch > 0)
        line.intField("completion_err_ms", record.completionErrMs);
    line.intField("awake_ms", record.awakeMs);

    line.floatField("device_v", record.volts, 3);
    line.floatField("device_pct", record.percent, 1);

    line.boolField("wifi_ok", record.wifiOk);
    line.intField("wifi_ms", record.wifiMs);
    line.boolField("wifi_cached", record.wifiCached);
    line.boolField("wifi_fallback", record.wifiFallback);
    line.intField("wifi_attempts", record.wifiAttempts);
    line.intField("wifi_status", record.wifiStatus);
    line.intField("wifi_reason", record.wifiReason);
    if (record.wifiOk) {
        line.intField("rssi", record.rssi);
        line.intField("channel", record.channel);
        line.stringField("bssid", record.bssid);
        line.stringField("ip", record.ip);
    }

    line.boolField("ntp_ran", record.ntpRan);
    line.boolField("ntp_ok", record.ntpOk);
    if (record.ntpRan) {
        line.intField("ntp_attempts", record.ntpAttempts);
        line.intField("ntp_ms", record.ntpMs);
    }
    if (record.ntpOk) line.intField("ntp_drift_ms", record.ntpDriftMs);
    if (record.syncAgeS >= 0) line.intField("sync_age_s", record.syncAgeS);
    line.boolField("clock_ok", record.clockOk);

    if (record.wifiOk && record.plan != Metrics::PlanCheckup) {
        line.boolField("fr_ok", record.frOk);
        line.intField("fr_attempts", record.frAttempts);
        line.intField("fr_ms", record.frMs);
        line.intField("fr_http", record.frHttp);
        line.intField("fr_bytes", record.frBytes);
        line.floatField("fr_pv_w", record.pvW, 2);
        line.floatField("fr_load_w", record.loadW, 2);
        line.floatField("fr_grid_w", record.gridW, 2);
        line.floatField("fr_batt_w", record.battW, 2);
        line.floatField("fr_soc", record.soc, 1);
        if (record.backup >= 0) line.intField("fr_backup", record.backup);

        line.boolField("wx_ok", record.wxOk);
        line.intField("wx_attempts", record.wxAttempts);
        line.intField("wx_ms", record.wxMs);
        line.intField("wx_http", record.wxHttp);
        line.intField("wx_bytes", record.wxBytes);
        line.floatField("wx_tmin_c", record.tminC, 1);
        line.floatField("wx_tmax_c", record.tmaxC, 1);
        line.floatField("wx_precip_pct", record.precipPct, 0);
        line.floatField("wx_code", record.code, 0);
    }

    if (record.plan == Metrics::PlanRefresh) {
        line.intField("prep_ms", record.prepMs);
        line.intField("refresh_ms", record.refreshMs);
        line.intField("wait_sleeps", record.waitSleeps);
        line.intField("wait_slept_ms", record.waitSleptMs);
        line.intField("ref_sleeps", record.refSleeps);
        line.intField("ref_slept_ms", record.refSleptMs);
        line.intField("sleep_fails", record.sleepFails);
    }

    line.floatField("chip_temp_c", record.chipTempC, 1);
    line.intField("heap_free", record.heapFree);
    line.intField("heap_min", record.heapMin);
    line.intField("heap_max", record.heapMax);

    line.intField("prev_post_ok", record.prevPostOk);
    line.intField("prev_post_ms", record.prevPostMs);

    if (record.wakeEpoch > 0) {
        char stamp[24];
        snprintf(stamp, sizeof(stamp), "%lld", static_cast<long long>(record.wakeEpoch));
        line.text += ' ';
        line.text += stamp;
    }
    fieldCount = line.fields;
    return line.text;
}

void appendQueryValue(String& url, const char* value) {
    char encoded[4];
    for (const char* p = value; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                          (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                          c == '.' || c == '~';
        if (safe) {
            url += static_cast<char>(c);
        } else {
            snprintf(encoded, sizeof(encoded), "%%%02X", c);
            url += encoded;
        }
    }
}

String writeUrl() {
    String url = Config::INFLUX_URL;
    while (url.endsWith("/")) url.remove(url.length() - 1);
    url += "/api/v2/write?";
    if (Config::INFLUX_ORG[0] != '\0') {
        url += "org=";
        appendQueryValue(url, Config::INFLUX_ORG);
        url += '&';
    }
    url += "bucket=";
    appendQueryValue(url, Config::INFLUX_BUCKET);
    url += "&precision=s";
    return url;
}

// One bounded POST: no retries here (the next cycle carries a fresh record
// anyway), so a dead server costs at most the connect/read timeouts.
bool postRecord(const Cycle& record, uint32_t& elapsedMs) {
    unsigned fields = 0;
    const String payload = buildLine(record, fields);

    WiFiClient client;
    HTTPClient http;
    http.useHTTP10(true);
    http.setReuse(false);
    http.setConnectTimeout(Timing::INFLUX_CONNECT_TIMEOUT_MS);
    http.setTimeout(Timing::INFLUX_READ_TIMEOUT_MS);

    const uint32_t start = millis();
    const String url = writeUrl();
    if (!http.begin(client, url)) {
        elapsedMs = millis() - start;
        Serial.println("Influx: cannot initialize HTTP");
        return false;
    }
    http.addHeader("Authorization", String("Token ") + Config::INFLUX_TOKEN);
    http.addHeader("Content-Type", "text/plain; charset=utf-8");
    const int status = http.POST(payload);
    elapsedMs = millis() - start;

    if (status >= 200 && status < 300) {
        Serial.printf("Influx: HTTP %d, %u fields, %u B, %lu ms\n", status, fields,
                      static_cast<unsigned>(payload.length()),
                      static_cast<unsigned long>(elapsedMs));
        http.end();
        return true;
    }
    if (status < 0) {
        Serial.printf("Influx: %s after %lu ms\n",
                      HTTPClient::errorToString(status).c_str(),
                      static_cast<unsigned long>(elapsedMs));
    } else {
        String body = http.getString();
        if (body.length() > 120) body.remove(120);
        body.trim();
        Serial.printf("Influx: HTTP %d after %lu ms%s%s\n", status,
                      static_cast<unsigned long>(elapsedMs),
                      body.length() ? ": " : "", body.c_str());
    }
    http.end();
    return false;
}
}  // namespace

namespace Metrics {
Cycle& current() { return currentCycle; }

void stage() {
    Cycle& record = current();
    if (Clock::valid()) {
        const int64_t now = Clock::nowMs();
        record.wakeEpoch = (now - static_cast<int64_t>(record.awakeMs)) / 1000;
    } else {
        record.wakeEpoch = 0;
    }
    memcpy(pendingBytes, &record, sizeof(record));
    pendingValid = true;
}

void resetPending() {
    pendingValid = false;
    lastPostOk = -1;
    lastPostMs = 0;
}

void postPending() {
    if (Config::INFLUX_URL[0] == '\0') return;  // telemetry disabled
    if (!pendingValid) return;
    if (Config::INFLUX_TOKEN[0] == '\0') {
        Serial.println("Influx: no token configured, skipping");
        return;
    }
    Cycle record;
    memcpy(&record, pendingBytes, sizeof(record));
    record.prevPostOk = lastPostOk;
    record.prevPostMs = lastPostMs;
    pendingValid = false;  // one shot; this cycle's record overwrites it anyway

    uint32_t elapsed = 0;
    const bool ok = postRecord(record, elapsed);

    lastPostOk = ok ? 1 : 0;
    lastPostMs = elapsed;
}
}  // namespace Metrics
