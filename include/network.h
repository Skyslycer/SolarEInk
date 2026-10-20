#pragma once
#include <stdint.h>
#include "readings.h"

namespace Network {
struct WifiStats {
    uint32_t attempts = 0;   // WiFi.begin() calls, cached attempt included
    uint32_t ms = 0;         // whole connectWifi() duration
    bool cachedUsed = false; // the cached-AP path was tried first
    bool cachedFailed = false;
    int status = 0;          // final wl_status_t
    int reason = 0;          // last disconnect reason (0 = none)
};
const WifiStats& lastWifiStats();

struct FetchStats {
    uint32_t attempts = 0;
    uint32_t ms = 0;         // total phase duration, retry delays included
    int http = 0;            // last attempt's status (negative = client error)
    uint32_t bytes = 0;      // last attempt's response body
};
const FetchStats& froniusStats();
const FetchStats& weatherStats();

bool connectWifi();
bool pollFronius(Readings::Fronius& reading);
bool pollWeather(Readings::Weather& reading);
}
