#pragma once
#include <cstdint>

namespace Clock {
struct SyncStats {
    bool ran = false;
    bool ok = false;
    uint32_t attempts = 0;
    uint32_t ms = 0;
    int64_t driftMs = 0;  // clock correction applied by the successful sync
};
void configureTimezone();
void syncIfNeeded(bool force = false);
bool valid();
int64_t nowMs();
int64_t lastSyncAgeSeconds();  // -1 when no sync is known
const SyncStats& lastSyncStats();
void format(int64_t epochMs, char (&text)[6]);
int localMinutes(int64_t epochMs);
int64_t nextLocalHourMs(int64_t afterMs, int hour);
}
