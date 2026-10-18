#pragma once

#include <ArduinoJson.h>
#include <cmath>

namespace Readings {
// NaN explicitly means missing/invalid; it must never become a zero reading.
inline double number(JsonVariantConst value) {
    if (!value.is<double>()) return NAN;
    const double result = value.as<double>();
    return std::isfinite(result) ? result : NAN;
}

inline double percent(JsonVariantConst value) {
    const double result = number(value);
    return result >= 0 && result <= 100 ? result : NAN;
}

struct Fronius {
    double pv = NAN;
    double grid = NAN;
    double load = NAN;
    double battery = NAN;
    double soc = NAN;
    bool backupKnown = false;
    bool backup = false;

    double consumption() const { return std::fabs(load); }
    bool complete() const {
        return std::isfinite(pv) && std::isfinite(grid) && std::isfinite(load) &&
               std::isfinite(battery) && std::isfinite(soc) && backupKnown;
    }
};

inline Fronius parseFronius(JsonVariantConst root) {
    Fronius result;
    // Some devices include an API error in an otherwise successful HTTP reply.
    const auto status = root["Head"]["Status"]["Code"];
    if (!status.isNull() && (!status.is<int>() || status.as<int>() != 0)) {
        return result;
    }
    const auto site = root["Body"]["Data"]["Site"];
    result.pv = number(site["P_PV"]);
    result.grid = number(site["P_Grid"]);
    result.load = number(site["P_Load"]);
    result.battery = number(site["P_Akku"]);
    result.soc = percent(root["Body"]["Data"]["Inverters"]["1"]["SOC"]);
    const auto backup = site["BackupMode"];
    if (backup.is<bool>()) {
        result.backupKnown = true;
        result.backup = backup.as<bool>();
    } else if (backup.is<int>() && (backup.as<int>() == 0 || backup.as<int>() == 1)) {
        result.backupKnown = true;
        result.backup = backup.as<int>() == 1;
    }
    return result;
}

struct Weather {
    double minimum = NAN;
    double maximum = NAN;
    double precipitation = NAN;
    double code = NAN;

    bool complete() const {
        return std::isfinite(minimum) && std::isfinite(maximum) &&
               std::isfinite(precipitation) && std::isfinite(code);
    }
};

inline Weather parseWeather(JsonVariantConst root) {
    Weather result;
    if (root["error"].as<bool>()) return result;
    const auto daily = root["daily"];
    result.minimum = number(daily["temperature_2m_min"][0]);
    result.maximum = number(daily["temperature_2m_max"][0]);
    result.precipitation = percent(daily["precipitation_probability_max"][0]);
    const double code = number(daily["weather_code"][0]);
    if (code >= 0 && code <= 99 && std::floor(code) == code) result.code = code;
    return result;
}

inline const char* gridDirection(double watts) {
    if (!std::isfinite(watts)) return "unavailable";
    return watts > 0 ? "import" : watts < 0 ? "export" : "idle";
}

inline const char* batteryDirection(double watts) {
    if (!std::isfinite(watts)) return "unavailable";
    return watts > 0 ? "discharge" : watts < 0 ? "charge" : "idle";
}
}  // namespace Readings
