#include <Arduino.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>
#include "app_config.h"
#include "clock_sync.h"

namespace {
RTC_DATA_ATTR time_t lastSync = 0;
volatile bool synchronized = false;
Clock::SyncStats syncStats;
void receivedTime(struct timeval*) { synchronized = true; }
}

namespace Clock {
void configureTimezone() {
    setenv("TZ", Timing::TIMEZONE, 1);
    tzset();
}
bool valid() { return time(nullptr) >= 1704067200; }  // 2024-01-01 UTC
int64_t nowMs() {
    timeval now;
    gettimeofday(&now, nullptr);
    return static_cast<int64_t>(now.tv_sec) * 1000 + now.tv_usec / 1000;
}
void syncIfNeeded(bool force) {
    const time_t now = time(nullptr);
    if (!force && valid() && lastSync > 0 && now >= lastSync &&
        now - lastSync < Timing::NTP_RESYNC_SECONDS) return;
    syncStats = SyncStats{};
    syncStats.ran = true;
    sntp_set_time_sync_notification_cb(receivedTime);
    const uint32_t start = millis();
    const int64_t wallStart = nowMs();
    unsigned attempts = 0;
    bool success = false;
    for (unsigned attempt = 0; attempt <= Timing::NTP_RETRIES; ++attempt) {
        if (attempt > 0) {
            Serial.printf("Time sync: retry %u/%u in %lu ms\n", attempt,
                          Timing::NTP_RETRIES,
                          static_cast<unsigned long>(Timing::NTP_RETRY_DELAY_MS));
            delay(Timing::NTP_RETRY_DELAY_MS);
        }
        synchronized = false;
        configTzTime(Timing::TIMEZONE, Timing::NTP_SERVER, "time.cloudflare.com");
        ++attempts;
        const uint32_t attemptStart = millis();
        while (!synchronized && millis() - attemptStart < Timing::NTP_TIMEOUT_MS) delay(20);
        if (synchronized && valid()) {
            lastSync = time(nullptr);
            success = true;
            // The clock jump applied by SNTP is the error accumulated since the
            // previous sync (plus the few milliseconds of callback/poll latency).
            syncStats.driftMs = (nowMs() - wallStart) -
                                static_cast<int64_t>(millis() - start);
            Serial.printf("Time sync: OK on attempt %u/%u, %lu ms\n", attempt + 1,
                          Timing::NTP_RETRIES + 1,
                          static_cast<unsigned long>(millis() - attemptStart));
            break;
        }
        Serial.printf("Time sync: attempt %u/%u failed after %lu ms\n", attempt + 1,
                      Timing::NTP_RETRIES + 1,
                      static_cast<unsigned long>(millis() - attemptStart));
        sntp_stop();
    }
    // Stop background SNTP before shutting down the radio. System/RTC time
    // continues through deep sleep, so a successful sync needs no repeat until
    // the six-hour resync interval expires.
    sntp_stop();
    syncStats.ok = success;
    syncStats.attempts = attempts;
    syncStats.ms = millis() - start;
    Serial.printf("Time sync: %s after %lu ms; clock %s\n",
                  success ? "OK" : "failed", static_cast<unsigned long>(millis() - start),
                  valid() ? "available" : "unavailable");
}
int64_t lastSyncAgeSeconds() {
    const time_t now = time(nullptr);
    if (lastSync <= 0 || now < lastSync) return -1;
    return static_cast<int64_t>(now - lastSync);
}
const SyncStats& lastSyncStats() { return syncStats; }
void format(int64_t epochMs, char (&text)[6]) {
    if (epochMs < 1704067200000LL) { snprintf(text, sizeof(text), "--:--"); return; }
    const time_t seconds = epochMs / 1000;
    tm local;
    localtime_r(&seconds, &local);
    strftime(text, sizeof(text), "%H:%M", &local);
}
int localMinutes(int64_t epochMs) {
    const time_t seconds = epochMs / 1000;
    tm local;
    if (epochMs < 1704067200000LL || !localtime_r(&seconds, &local)) return -1;
    return local.tm_hour * 60 + local.tm_min;
}
int64_t nextLocalHourMs(int64_t afterMs, int hour) {
    time_t seconds = afterMs / 1000;
    tm local;
    if (afterMs < 1704067200000LL || !localtime_r(&seconds, &local)) return 0;
    local.tm_hour = hour;
    local.tm_min = 0;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    time_t when = mktime(&local);
    if (when == static_cast<time_t>(-1)) return 0;
    if (static_cast<int64_t>(when) * 1000 <= afterMs) {
        local.tm_mday += 1;
        local.tm_isdst = -1;
        when = mktime(&local);
        if (when == static_cast<time_t>(-1)) return 0;
    }
    return static_cast<int64_t>(when) * 1000;
}
}
