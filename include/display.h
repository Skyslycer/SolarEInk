#pragma once
#include <stdint.h>
#include "dashboard.h"

namespace Screen {
struct EpaperStats {
    uint32_t sleeps = 0;   // light-sleep slices taken while the panel was busy
    uint32_t sleptMs = 0;
    uint32_t fails = 0;    // light sleep refused; fell back to delay()
};
const EpaperStats& epaperStats();
void prepare(const Dashboard& data);
uint32_t refreshAndSleep();
}
