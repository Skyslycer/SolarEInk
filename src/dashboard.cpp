#include "dashboard.h"
#include "fonts/dashboard_fonts.h"
#include "logic.h"
#include "icons.h"
#include <cstdio>
#include <cstring>

namespace {
constexpr uint16_t BLACK = 0x0000, WHITE = 0xffff, RED = 0xf800;

void text(Adafruit_GFX& g, const char* value, int x, int y, const GFXfont* font,
          uint16_t color = BLACK, int maxWidth = 0, bool center = false) {
    g.setFont(font);
    g.setTextSize(1);
    g.setTextColor(color);
    int16_t bx, by;
    uint16_t w, h;
    g.getTextBounds(value, 0, 0, &bx, &by, &w, &h);
    if (maxWidth && w > maxWidth) {
        g.setFont(font == &FontLarge ? &FontMedium : &FontTiny);
        g.getTextBounds(value, 0, 0, &bx, &by, &w, &h);
        if (w > maxWidth) {
            g.setFont(&FontTiny);
            g.getTextBounds(value, 0, 0, &bx, &by, &w, &h);
        }
    }
    g.setCursor(x - bx + (center ? (maxWidth - w) / 2 : 0), y - by);
    g.print(value);
}

void power(Adafruit_GFX& g, double watts, int x, int y, int width,
           const GFXfont* font = &FontMedium, uint16_t color = BLACK, bool center = false) {
    char buffer[24];
    Logic::formatPower(watts, buffer, sizeof(buffer));
    if (!std::isfinite(watts)) {
        text(g, buffer, x, y, font, color, width, center);
        return;
    }
    char* unit = std::strstr(buffer, "kW");
    const bool kilo = unit != nullptr;
    if (!unit) unit = std::strchr(buffer, 'W');
    if (!unit) { // Reject an out-of-range value that overflowed the text buffer.
        text(g, "--", x, y, font, color, width, center);
        return;
    }
    *unit = '\0';
    const char* units = kilo ? "kW" : "W";
    int16_t bx, by;
    uint16_t numberWidth, numberHeight, unitWidth, unitHeight;
    g.setFont(&FontTiny);
    g.getTextBounds(units, 0, 0, &bx, &by, &unitWidth, &unitHeight);
    g.setFont(font);
    g.getTextBounds(buffer, 0, 0, &bx, &by, &numberWidth, &numberHeight);
    if (numberWidth + 2 + unitWidth > width && font == &FontLarge) {
        font = &FontMedium;
        g.setFont(font);
        g.getTextBounds(buffer, 0, 0, &bx, &by, &numberWidth, &numberHeight);
    }
    // Prefer dropping an unnecessary decimal to making the digits tiny.
    if (numberWidth + 2 + unitWidth > width && kilo) {
        snprintf(buffer, sizeof(buffer), "%.1f", std::fabs(watts) / 1000.0);
        g.getTextBounds(buffer, 0, 0, &bx, &by, &numberWidth, &numberHeight);
    }
    if (numberWidth + 2 + unitWidth > width) {
        font = &FontTiny;
        g.setFont(font);
        g.getTextBounds(buffer, 0, 0, &bx, &by, &numberWidth, &numberHeight);
    }
    if (center) x += (width - numberWidth - 2 - unitWidth) / 2;
    text(g, buffer, x, y, font, color);
    text(g, units, x + numberWidth + 2, y + numberHeight - unitHeight, &FontTiny, color);
}

int direction(double value) {
    return !std::isfinite(value) || value == 0 ? 0 : value > 0 ? 1 : -1;
}

void status(Adafruit_GFX& g, const Dashboard& d) {
    // Wi-Fi connection status, device LiPo, and the local clock.
    Icons::wifi(g, d.wifi);
    g.drawFastVLine(216, 12, 15, BLACK);
    Icons::deviceBattery(g, d.devicePercent);
    char percent[8];
    if (std::isfinite(d.devicePercent)) snprintf(percent, sizeof(percent), "%.0f%%", d.devicePercent);
    else snprintf(percent, sizeof(percent), "--%%");
    text(g, percent, 234, 17, &FontTiny, BLACK, 23);
    g.drawFastVLine(260, 12, 15, BLACK);
    text(g, d.clock, 265, 17, &FontTiny, BLACK, 30);
}
}

void drawDashboard(Adafruit_GFX& g, const Dashboard& d) {
    g.fillScreen(WHITE);
    g.setTextWrap(false);
    const auto weather = Logic::weatherIcon(d.weather.code);
    if (weather == Logic::WeatherIcon::Unknown) text(g, "?", 12, 17, &FontMedium);
    else Icons::weather(g, weather);
    g.drawFastVLine(33, 12, 29, BLACK);
    char temperatures[24], minimum[8], maximum[8];
    if (std::isfinite(d.weather.minimum)) snprintf(minimum, sizeof(minimum), "%.0f", d.weather.minimum);
    else snprintf(minimum, sizeof(minimum), "--");
    if (std::isfinite(d.weather.maximum)) snprintf(maximum, sizeof(maximum), "%.0f", d.weather.maximum);
    else snprintf(maximum, sizeof(maximum), "--");
    snprintf(temperatures, sizeof(temperatures), "%s\x7f/%s\x7f", minimum, maximum);
    text(g, temperatures, 38, 14, &FontLabel, BLACK, 57);
    Icons::umbrella(g, 38, 28);
    char rain[8];
    if (std::isfinite(d.weather.precipitation)) snprintf(rain, sizeof(rain), "%.0f%%", d.weather.precipitation);
    else snprintf(rain, sizeof(rain), "--%%");
    text(g, rain, 53, 30, &FontLabel);

    Icons::solar(g);
    text(g, "SOLAR", 146, 14, &FontLabel);
    power(g, d.solar.pv, 145, 27, 50);
    if (std::isfinite(d.solar.pv)) {
        Icons::verticalArrow(g, 43, 57, 145, d.solar.pv > 0 ? 1 : 0);
    }
    status(g, d);

    if (d.solar.backupKnown && d.solar.backup) {
        g.fillRect(5, 74, 80, 44, RED);
        text(g, "NOTSTROM", 8, 83, &FontLabel, WHITE, 74, true);
        text(g, "AKTIV", 8, 98, &FontMedium, WHITE, 74, true);
        Icons::openSwitch(g);
    } else {
        Icons::pylon(g);
        text(g, "NETZ", 40, 75, &FontLabel);
        power(g, d.solar.grid, 40, 87, 66);
        if (std::isfinite(d.solar.grid)) Icons::horizontalArrow(g, 36, 104, 106, direction(d.solar.grid));
        if (!d.solar.backupKnown) text(g, "?", 101, 75, &FontTiny, RED);
    }
    Icons::house(g);
    text(g, "HAUS", 116, 82, &FontLabel, BLACK, 59, true);
    power(g, d.solar.consumption(), 114, 95, 63, &FontLarge, RED, true);
    if (std::isfinite(d.solar.battery)) Icons::horizontalArrow(g, 186, 207, 106, -direction(d.solar.battery));
    Icons::storage(g, d.solar.soc);
    if (!std::isfinite(d.solar.soc)) text(g, "?", 218, 91, &FontMedium);
    text(g, "SPEICHER", 240, 75, &FontTiny, BLACK, 55);
    char soc[8];
    if (std::isfinite(d.solar.soc)) snprintf(soc, sizeof(soc), "%.0f%%", d.solar.soc);
    else snprintf(soc, sizeof(soc), "--%%");
    text(g, soc, 240, 87, &FontMedium, BLACK, 55);
    const int flow = direction(d.solar.battery);
    if (flow) {
        const uint16_t color = flow > 0 ? RED : BLACK;
        Icons::verticalArrow(g, 107, 120, 243, -flow, color);
    }
    power(g, d.solar.battery, 250, 107, 45);
}
