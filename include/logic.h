#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace Logic {
inline void formatPower(double watts, char* out, size_t length) {
    if (!std::isfinite(watts)) { snprintf(out, length, "--"); return; }
    watts = std::fabs(watts);
    if (watts < 999.5) snprintf(out, length, "%.0fW", watts);
    else snprintf(out, length, watts < 9995 ? "%.2fkW" : "%.1fkW", watts / 1000.0);
}

// Approximate rested single-cell LiPo curve, not a coulomb-counting fuel gauge.
inline double batteryPercent(double voltage) {
    if (!std::isfinite(voltage) || voltage < 2.7 || voltage > 4.35) return NAN;
    constexpr double volts[] = {3.20, 3.50, 3.60, 3.70, 3.75, 3.80, 3.85, 3.90, 4.00, 4.10, 4.20};
    constexpr double soc[] = {0, 5, 10, 20, 35, 50, 60, 70, 80, 90, 100};
    if (voltage <= volts[0]) return 0;
    for (unsigned i = 1; i < sizeof(volts) / sizeof(volts[0]); ++i) {
        if (voltage <= volts[i])
            return soc[i-1] + (voltage-volts[i-1]) * (soc[i]-soc[i-1]) / (volts[i]-volts[i-1]);
    }
    return 100;
}

inline int batteryBars(double soc) {
    return std::isfinite(soc) ? static_cast<int>(std::ceil(std::fmax(0, std::fmin(100, soc)) / 25)) : 0;
}

enum class WeatherIcon { Sun, MostlySunny, PartlyCloudy, Cloud, Fog, Drizzle, Rain,
                         FreezingRain, Snow, Showers, SnowShowers, Storm, Hail, Unknown };
inline WeatherIcon weatherIcon(double code) {
    if (!std::isfinite(code)) return WeatherIcon::Unknown;
    switch (static_cast<int>(code)) {
        case 0: return WeatherIcon::Sun;
        case 1: return WeatherIcon::MostlySunny;
        case 2: return WeatherIcon::PartlyCloudy;
        case 3: return WeatherIcon::Cloud;
        case 45: case 48: return WeatherIcon::Fog;
        case 51: case 53: case 55: return WeatherIcon::Drizzle;
        case 56: case 57: case 66: case 67: return WeatherIcon::FreezingRain;
        case 61: case 63: case 65: return WeatherIcon::Rain;
        case 71: case 73: case 75: case 77: return WeatherIcon::Snow;
        case 80: case 81: case 82: return WeatherIcon::Showers;
        case 85: case 86: return WeatherIcon::SnowShowers;
        case 95: return WeatherIcon::Storm;
        case 96: case 99: return WeatherIcon::Hail;
        default: return WeatherIcon::Unknown;
    }
}

// Epoch milliseconds, rounded to the next whole refresh interval.
inline int64_t nextBoundary(int64_t earliestMs, uint32_t intervalSeconds) {
    const int64_t period = static_cast<int64_t>(intervalSeconds) * 1000;
    return ((earliestMs + period - 1) / period) * period;
}

// Chooses the interval boundary this wake refreshes for.
// carriedMs is the boundary inherited from the previous cycle (0 when none).
struct TargetPlan {
    int64_t targetMs;   // Boundary whose minute the refresh will display
    int64_t missedMs;   // Carried boundary that arrived too late (0 if none)
    bool refreshNow;    // false: deep-sleep through to the target's wake
};

inline TargetPlan planTarget(int64_t nowMs, int64_t carriedMs, int64_t refreshMs,
                             uint32_t intervalSeconds, int64_t lateToleranceMs) {
    const int64_t earliest = nowMs + refreshMs;
    if (carriedMs == 0 || carriedMs > earliest + static_cast<int64_t>(intervalSeconds) * 1000)
        return {nextBoundary(earliest + 200, intervalSeconds), 0, false};
    // A slow request can push completion past the boundary. Up to the late
    // tolerance the refresh still runs with the target's minute; later it is
    // skipped so the device can sleep through to the next interval.
    if (earliest > carriedMs + lateToleranceMs)
        return {nextBoundary(earliest + 200, intervalSeconds), carriedMs, false};
    return {carriedMs, 0, true};
}

// True when a local time of day, in minutes since midnight, is inside the
// daily update window (firstHour:00 through lastHour:00 inclusive).
inline bool inDailyWindow(int minutesSinceMidnight, int firstHour, int lastHour) {
    return minutesSinceMidnight >= firstHour * 60 && minutesSinceMidnight <= lastHour * 60;
}
}
