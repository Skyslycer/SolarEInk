#pragma once

#include <Arduino.h>
#include <cmath>
#include <cstdint>

// Per-cycle telemetry. A record is filled while the cycle runs, completed just
// before deep sleep, kept in RTC memory, and posted on the next cycle while
// Wi-Fi is already up. Keeping the struct free of heap allocations is essential
// because its RTC copy must survive deep sleep unchanged.
namespace Metrics {
enum Plan : int {
    PlanRefresh = 0,   // full cycle with a screen update
    PlanNoTarget = 1,  // no clock/target yet; slept through
    PlanTooLate = 2,   // carried boundary missed; slept through
    PlanCheckup = 3,   // night-time NTP-only checkup
};

struct Cycle {
    // scheduler / boot
    uint32_t bootCount = 0;
    int wakeCause = 0;
    int resetReason = 0;
    bool checkup = false;
    bool usb = false;
    int plan = PlanNoTarget;
    int64_t wakeEpoch = 0;      // seconds; the point's timestamp (0 = unknown)
    int64_t targetEpoch = 0;    // seconds; 0 when there was no target
    int64_t missedMs = 0;
    int64_t completionErrMs = 0;
    uint32_t awakeMs = 0;

    // battery
    double volts = NAN;
    double percent = NAN;

    // Wi-Fi
    bool wifiOk = false;
    uint32_t wifiMs = 0;
    bool wifiCached = false;
    bool wifiFallback = false;
    uint32_t wifiAttempts = 0;
    int wifiStatus = 0;
    int wifiReason = 0;
    int rssi = 0;
    int channel = 0;
    char bssid[18] = "";
    char ip[16] = "";

    // NTP
    bool ntpRan = false;
    bool ntpOk = false;
    uint32_t ntpAttempts = 0;
    uint32_t ntpMs = 0;
    int64_t ntpDriftMs = 0;  // clock correction applied by the successful sync
    int64_t syncAgeS = -1;   // age of the previous sync at wake
    bool clockOk = false;

    // Fronius
    bool frOk = false;
    uint32_t frAttempts = 0;
    uint32_t frMs = 0;
    int frHttp = 0;
    uint32_t frBytes = 0;
    double pvW = NAN;
    double loadW = NAN;
    double gridW = NAN;
    double battW = NAN;
    double soc = NAN;
    int backup = -1;  // -1 unknown, 0 inactive, 1 active

    // Open-Meteo
    bool wxOk = false;
    uint32_t wxAttempts = 0;
    uint32_t wxMs = 0;
    int wxHttp = 0;
    uint32_t wxBytes = 0;
    double tminC = NAN;
    double tmaxC = NAN;
    double precipPct = NAN;
    double code = NAN;

    // panel / light sleep
    uint32_t prepMs = 0;
    uint32_t refreshMs = 0;
    uint32_t waitSleeps = 0;
    uint32_t waitSleptMs = 0;
    uint32_t refSleeps = 0;
    uint32_t refSleptMs = 0;
    uint32_t sleepFails = 0;

    // system
    float chipTempC = NAN;
    uint32_t heapFree = 0;
    uint32_t heapMin = 0;
    uint32_t heapMax = 0;

    // telemetry meta, filled when this record is posted
    int prevPostOk = -1;
    uint32_t prevPostMs = 0;
};

Cycle& current();
void resetPending();  // drop stale RTC state on cold boot / reflash
void postPending();   // call while Wi-Fi is connected; posts the previous cycle
void stage();         // call before deep sleep; hands this cycle to the next boot
}  // namespace Metrics
