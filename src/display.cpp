#include <GxEPD2_3C.h>
#include <SPI.h>
#include <esp_sleep.h>
#include "app_config.h"
#include "display.h"

namespace {
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(
    GxEPD2_290_C90c(Hardware::EPD_CS, Hardware::EPD_DC, Hardware::EPD_RST, Hardware::EPD_BUSY));

// GxEPD2 polls BUSY with delay(1) while the panel runs its ~27 s waveform,
// holding the CPU at full clock. With no USB host attached, sleep through the
// wait in short slices instead: light sleep keeps the GPIOs driven and
// micros()/the wall clock keep counting, so the BUSY timeout and the measured
// refresh duration stay correct. With USB attached, fall back to delay(1) so
// the Serial/JTAG link survives and bench behavior is unchanged.
void panelBusySleep(const void*) {
    if (Serial) {
        delay(1);
        return;
    }
    esp_sleep_enable_timer_wakeup(Timing::PANEL_BUSY_SLEEP_SLICE_US);
    if (esp_light_sleep_start() != ESP_OK) delay(1);
}
}

namespace Screen {
void prepare(const Dashboard& data) {
    SPI.begin(Hardware::EPD_SCK, -1, Hardware::EPD_MOSI, Hardware::EPD_CS);
    display.init(115200, true, 50, false, SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.epd2.setBusyCallback(panelBusySleep);
    display.setRotation(Hardware::DISPLAY_ROTATION);
    drawDashboard(display, data);
}

uint32_t refreshAndSleep() {
    const uint32_t start = millis();
    display.display(false);  // Full color update; this panel has no fast partial update.
    display.hibernate();
    SPI.end();
    const uint32_t elapsed = millis() - start;
    Serial.printf("E-paper: refreshed and hibernated in %lu ms\n", static_cast<unsigned long>(elapsed));
    return elapsed;
}
}
