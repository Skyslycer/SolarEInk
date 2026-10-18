#pragma once
#include <cstdint>

namespace Clock {
void configureTimezone();
void syncIfNeeded(bool force = false);
bool valid();
int64_t nowMs();
void format(int64_t epochMs, char (&text)[6]);
int localMinutes(int64_t epochMs);
int64_t nextLocalHourMs(int64_t afterMs, int hour);
}
