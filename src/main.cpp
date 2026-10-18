#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>
#include "app_config.h"
#include "battery.h"
#include "clock_sync.h"
#include "display.h"
#include "logic.h"
#include "network.h"

RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR int64_t previousAwakeUs = 0;
RTC_DATA_ATTR int64_t nextRefreshMs = 0;
RTC_DATA_ATTR uint32_t refreshEstimateMs = Timing::INITIAL_REFRESH_MS;
RTC_DATA_ATTR uint32_t prepareEstimateMs = Timing::INITIAL_PREPARE_MS;
RTC_DATA_ATTR bool checkupPending = false;

namespace {
// Alignment waits (from "image ready" until the refresh must start) run with
// the CPU awake otherwise. When no USB host is attached, light-sleep through
// them in short slices; with a monitor attached keep the plain delay so the
// bench behaves exactly as before. The wall clock keeps counting in light
// sleep, so the loop still starts the refresh exactly on the target.
void waitSlice(uint32_t ms) {
    if (Serial) {
        delay(ms);
        return;
    }
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(ms) * 1000);
    if (esp_light_sleep_start() != ESP_OK) delay(ms);
}

void deepSleepFor(int64_t sleepUs) {
    Serial.printf("Wi-Fi off. Total awake so far: %.3f ms; deep sleep: %.3f s\n",
                  esp_timer_get_time() / 1000.0, sleepUs / 1000000.0);
    const int64_t flushStart = esp_timer_get_time();
    Serial.flush();
    sleepUs = std::max<int64_t>(1000, sleepUs - (esp_timer_get_time() - flushStart));
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(sleepUs));
    previousAwakeUs = esp_timer_get_time();
    esp_deep_sleep_start();
}
// Advance a boundary until its local time of day is inside the daily update
// window, so nothing refreshes during the overnight quiet hours. Bounded to a
// full day so an unexpected local-time failure can never park the device.
int64_t nextAllowedTargetMs(int64_t targetMs) {
    constexpr unsigned maxSteps = 24 * 60 * 60 / Config::REFRESH_SECONDS;
    for (unsigned step = 0;
         step < maxSteps && targetMs &&
         !Logic::inDailyWindow(Clock::localMinutes(targetMs),
                               Config::FIRST_UPDATE_HOUR, Config::LAST_UPDATE_HOUR);
         ++step)
        targetMs += Config::REFRESH_SECONDS * 1000LL;
    return targetMs;
}
}  // namespace

void setup() {
    const auto wakeCause = esp_sleep_get_wakeup_cause();
    ++bootCount;
    Serial.begin(115200);
    Serial.setTxTimeoutMs(Config::SERIAL_TX_TIMEOUT_MS);
    const uint32_t serialStart = millis();
    while (!Serial && millis() - serialStart < Config::SERIAL_WAIT_MS) delay(10);
    const bool checkup = checkupPending && wakeCause == ESP_SLEEP_WAKEUP_TIMER;
    checkupPending = false;
    Serial.printf("\nBoot #%lu, wake=%s (%d), reset=%d%s\n",
                  static_cast<unsigned long>(bootCount),
                  wakeCause == ESP_SLEEP_WAKEUP_TIMER ? "deep-sleep timer" : "power/reset/other",
                  static_cast<int>(wakeCause), static_cast<int>(esp_reset_reason()),
                  checkup ? " [night checkup]" : "");
    if (wakeCause == ESP_SLEEP_WAKEUP_TIMER && previousAwakeUs > 0)
        Serial.printf("Previous cycle awake through final Serial flush: %.3f ms\n", previousAwakeUs / 1000.0);

    Clock::configureTimezone();
    if (checkup) {
        const bool wifi = Network::connectWifi();
        if (wifi) Clock::syncIfNeeded(true);
        WiFi.disconnect(true, false);
        WiFi.mode(WIFI_OFF);
        int64_t sleepUs = 0;
        char target[6];
        Clock::format(nextRefreshMs, target);
        if (Clock::valid() && nextRefreshMs > 0) {
            const int64_t morningWakeMs = nextRefreshMs - Config::MORNING_WAKE_BEFORE_MINUTES * 60000LL;
            if (morningWakeMs > Clock::nowMs() + 1000) sleepUs = (morningWakeMs - Clock::nowMs()) * 1000;
        }
        Serial.printf("Night checkup: Wi-Fi=%s, clock %s\n", wifi ? "OK" : "FAILED",
                      Clock::valid() ? "corrected" : "unavailable");
        if (sleepUs > 0) {
            Serial.printf("Sleeping until %d min before the %s update\n",
                          Config::MORNING_WAKE_BEFORE_MINUTES, target);
            deepSleepFor(sleepUs);
        }
        // The morning wake is already due: fall through to a normal cycle.
    }
    Dashboard data;
    data.deviceVolts = readDeviceBatteryVolts();
    data.devicePercent = Logic::batteryPercent(data.deviceVolts);
    data.wifi = Network::connectWifi();
    bool solarOk = false, weatherOk = false;
    if (data.wifi) {
        data.rssi = WiFi.RSSI();
        Clock::syncIfNeeded();
        solarOk = Network::pollFronius(data.solar);
        weatherOk = Network::pollWeather(data.weather);
    }
    Serial.printf("Cycle: Wi-Fi=%s Fronius=%s weather=%s\n", data.wifi ? "OK" : "FAILED",
                  data.wifi ? (solarOk ? "OK" : "FAILED/INCOMPLETE") : "SKIPPED",
                  data.wifi ? (weatherOk ? "OK" : "FAILED/INCOMPLETE") : "SKIPPED");
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);

    // Aim for the image to FINISH updating on each configured interval
    // boundary (for example 13:00, 13:15, 13:30). Use the previous actual time.
    // When the target is unreachable, sleep through to the next interval
    // instead of refreshing a stale minute or waiting awake for the next one.
    int64_t targetMs = 0;
    Logic::TargetPlan plan{0, 0, true};
    if (Clock::valid()) {
        plan = Logic::planTarget(Clock::nowMs(), nextRefreshMs, refreshEstimateMs,
                                 Config::REFRESH_SECONDS, Timing::TARGET_LATE_TOLERANCE_MS);
        const int64_t allowedMs = nextAllowedTargetMs(plan.targetMs);
        if (allowedMs != plan.targetMs) plan = {allowedMs, plan.missedMs, false};
        targetMs = plan.targetMs;
        Clock::format(targetMs, data.clock);
    }
    uint32_t prepareMs;
    if (!plan.refreshNow) {
        prepareMs = esp_timer_get_time() / 1000;
        if (plan.missedMs) {
            char missed[6];
            Clock::format(plan.missedMs, missed);
            Serial.printf("Too late for the %s update; sleeping through to %s\n",
                          missed, data.clock);
        } else {
            Serial.printf("No target yet; sleeping through to the %s update\n", data.clock);
        }
    } else {
        Screen::prepare(data);
        prepareMs = esp_timer_get_time() / 1000;
        if (targetMs) {
            int64_t waitMs = targetMs - refreshEstimateMs - Clock::nowMs();
            Serial.printf("Display target %s; starting refresh in %lld ms (estimate %lu ms)\n",
                          data.clock, std::max(0LL, static_cast<long long>(waitMs)),
                          static_cast<unsigned long>(refreshEstimateMs));
            while (waitMs > 0) {
                waitSlice(static_cast<uint32_t>(std::min<int64_t>(waitMs, 100)));
                waitMs = targetMs - refreshEstimateMs - Clock::nowMs();
            }
        } else {
            Serial.println("No synchronized clock: displaying --:-- and using relative wake timing");
        }
        const uint32_t actualRefreshMs = Screen::refreshAndSleep();
        if (actualRefreshMs >= 5000 && actualRefreshMs <= 55000) refreshEstimateMs = actualRefreshMs;
        if (targetMs)
            Serial.printf("Refresh completion error: %+lld ms from target\n",
                          static_cast<long long>(Clock::nowMs() - targetMs));
    }
    // Include application startup, ADC, Wi-Fi and requests, plus framebuffer
    // setup when the refresh runs. Exclude alignment waits and display waveform.
    prepareEstimateMs = prepareMs;

    int64_t sleepUs;
    if (targetMs) {
        const int64_t now = Clock::nowMs();
        const int64_t leadMs = prepareEstimateMs + refreshEstimateMs + Timing::WAKE_MARGIN_MS;
        // A skipped cycle wakes directly for its target's full cycle.
        nextRefreshMs = plan.refreshNow ? targetMs + Config::REFRESH_SECONDS * 1000LL : targetMs;
        while (nextRefreshMs - leadMs <= now + 1000)
            nextRefreshMs += Config::REFRESH_SECONDS * 1000LL;
        nextRefreshMs = nextAllowedTargetMs(nextRefreshMs);
        int64_t wakeMs = nextRefreshMs - leadMs;
        // Break up the overnight sleep with a time-sync checkup so internal RC
        // drift cannot push the first morning update out of its slot.
        const int64_t checkupMs = Clock::nextLocalHourMs(now, Config::CHECKUP_HOUR);
        if (checkupMs > now + 1000 && checkupMs < wakeMs) {
            checkupPending = true;
            wakeMs = checkupMs;
        }
        sleepUs = (wakeMs - now) * 1000;
    } else {
        nextRefreshMs = 0;
        sleepUs = std::max<int64_t>(1000000, Config::REFRESH_SECONDS * 1000000LL - esp_timer_get_time());
    }
    deepSleepFor(sleepUs);
}

void loop() {}
