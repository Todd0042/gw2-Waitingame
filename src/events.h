#pragma once
#include <cstdint>

struct EventCountdown {
    const char* name;      // event name (pointer into static table, always valid)
    const char* mapName;   // map name
    int         secondsUntil;  // seconds until next occurrence; 0 = starting now
    bool        valid;     // false when on an untracked map (not a city, not an event map)
};

// Returns the event scheduled for this specific map, or valid=false if none.
EventCountdown Events_GetForMap(uint32_t mapId);

// Returns the soonest upcoming event across all tracked maps.
// Used when the player is in a city with no map-specific event.
EventCountdown Events_GetNextAny();

bool Events_IsCityMap(uint32_t mapId);
