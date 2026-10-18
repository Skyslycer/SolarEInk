#pragma once
#include "readings.h"

namespace Network {
bool connectWifi();
bool pollFronius(Readings::Fronius& reading);
bool pollWeather(Readings::Weather& reading);
}
