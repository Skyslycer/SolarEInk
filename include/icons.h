#pragma once
#include <Adafruit_GFX.h>
#include "logic.h"

namespace Icons {
void weather(Adafruit_GFX& g, Logic::WeatherIcon icon);
void umbrella(Adafruit_GFX& g, int x, int y);
void solar(Adafruit_GFX& g);
void pylon(Adafruit_GFX& g);
void house(Adafruit_GFX& g);
void storage(Adafruit_GFX& g, double soc);
void deviceBattery(Adafruit_GFX& g, double percent);
void wifi(Adafruit_GFX& g, bool connected);
void openSwitch(Adafruit_GFX& g);
// Positive points right/down; negative points left/up; zero is a plain line.
void horizontalArrow(Adafruit_GFX& g, int left, int right, int y, int direction);
void verticalArrow(Adafruit_GFX& g, int top, int bottom, int x, int direction,
                   uint16_t color = 0x0000);
}
