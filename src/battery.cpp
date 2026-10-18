#include <Arduino.h>
#include <algorithm>
#include "app_config.h"
#include "battery.h"

double readDeviceBatteryVolts() {
    // Run before Wi-Fi/display initialization. The divider's Thevenin R is
    // 110k, so 100nF gives an 11ms time constant; allow >5 time constants.
    pinMode(Hardware::BATTERY_ADC, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(Hardware::BATTERY_ADC, ADC_11db);
    analogReadMilliVolts(Hardware::BATTERY_ADC);
    delay(60);
    uint32_t samples[Hardware::BATTERY_SAMPLES];
    for (auto& sample : samples) {
        sample = analogReadMilliVolts(Hardware::BATTERY_ADC);
        delay(2);
    }
    std::sort(samples, samples + Hardware::BATTERY_SAMPLES);
    double sum = 0;
    // Trim the four highest and lowest samples.
    for (int i = 4; i < Hardware::BATTERY_SAMPLES - 4; ++i) sum += samples[i];
    const double voltage = sum / (Hardware::BATTERY_SAMPLES - 8) / 1000.0 *
                           Hardware::BATTERY_DIVIDER * Hardware::BATTERY_CALIBRATION;
    Serial.printf("Device battery: %.3f V (32 samples before Wi-Fi)\n", voltage);
    return voltage;
}
