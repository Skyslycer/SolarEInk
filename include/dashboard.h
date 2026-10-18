#pragma once
#include <Adafruit_GFX.h>
#include "readings.h"

struct Dashboard {
    Readings::Fronius solar;
    Readings::Weather weather;
    bool wifi = false;
    int rssi = -100;
    double deviceVolts = NAN;
    double devicePercent = NAN;
    char clock[6] = "--:--";
};

void drawDashboard(Adafruit_GFX& canvas, const Dashboard& data);
