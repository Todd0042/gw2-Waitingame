#include "events.h"
#include <ctime>
#include <climits>

// All map IDs verified via https://api.guildwars2.com/v2/maps
// All timings verified via GW2-Bosses addon source and GW2-Simple-Timer data
// (https://github.com/Sognus/Gw2-Bosses, https://github.com/Snipppppy/GW2-Simple-Timer)
//
// Two scheduling modes:
//   intervalMinutes > 0  ->  uniform cycle: fires every intervalMinutes starting at offsetMinutes
//   intervalMinutes == 0 ->  irregular: fires at each entry in times[] (minutes from midnight UTC)

struct EventEntry {
    uint32_t    mapId;
    const char* name;
    const char* mapName;
    int         intervalMinutes; // 0 = use times[] below
    int         offsetMinutes;   // only used when intervalMinutes > 0
    const int*  times;           // daily minute offsets; nullptr when intervalMinutes > 0
    int         timesCount;
};

// Irregular daily schedules (minutes from midnight UTC)
static const int s_tequatlTimes[]    = { 0, 180, 420, 690, 960, 1140 };        // 00:00 03:00 07:00 11:30 16:00 19:00
static const int s_tripleTroubleTimes[] = { 60, 240, 480, 750, 1020, 1200 };   // 01:00 04:00 08:00 12:30 17:00 20:00
static const int s_karkaQueenTimes[] = { 120, 360, 630, 900, 1080, 1380 };     // 02:00 06:00 10:30 15:00 18:00 23:00

static const EventEntry s_events[] = {
    // ---- Core world bosses — 2-hour cycle --------------------------------
    {  28, "Svanir Shaman Chief",  "Wayfarer Foothills",   120,  15, nullptr, 0 },
    {  35, "Fire Elemental",       "Metrica Province",     120,  45, nullptr, 0 },
    {  34, "Great Jungle Wurm",    "Caledon Forest",       120,  75, nullptr, 0 },
    {  15, "Shadow Behemoth",      "Queensdale",           120, 105, nullptr, 0 },

    // ---- Core world bosses — 3-hour cycle --------------------------------
    {  39, "Megadestroyer",        "Mount Maelstrom",      180,  30, nullptr, 0 },
    {  20, "The Shatterer",        "Blazeridge Steppes",   180,  60, nullptr, 0 },
    {  17, "Modniir Ulgoth",       "Harathi Hinterlands",  180,  90, nullptr, 0 },
    {  54, "Golem Mark II",        "Brisban Wildlands",    180, 120, nullptr, 0 },
    {  30, "Claw of Jormag",       "Frostgorge Sound",     180, 150, nullptr, 0 },

    // ---- Core world bosses — irregular daily schedule --------------------
    {  53, "Tequatl the Sunless",  "Sparkfly Fen",           0,   0, s_tequatlTimes,     6 },
    {  73, "Triple Trouble",       "Bloodtide Coast",        0,   0, s_tripleTroubleTimes, 6 },
    {  73, "Taidha Covington",     "Bloodtide Coast",      180,   0, nullptr, 0 },  // same map as TT; soonest shown
    { 873, "Karka Queen",          "Southsun Cove",          0,   0, s_karkaQueenTimes,  6 },

    // ---- Heart of Thorns metas — 2-hour cycle ----------------------------
    { 1042, "Night Bosses",        "Verdant Brink",        120,  10, nullptr, 0 },
    { 1045, "Chak Gerent",         "Tangled Depths",       120,  30, nullptr, 0 },
    { 1043, "Octovine",            "Auric Basin",          120,  60, nullptr, 0 },
    { 1041, "Dragon's Stand",      "Dragon's Stand",       120,  90, nullptr, 0 },

    // ---- Path of Fire metas — 2-hour cycle -------------------------------
    { 1210, "Casino Blitz",        "Crystal Oasis",        120,   5, nullptr, 0 },
    { 1248, "Serpent's Ire",       "Domain of Vabbi",      120,  30, nullptr, 0 },
    { 1226, "Maws of Torment",     "The Desolation",       120,  60, nullptr, 0 },
    { 1301, "DB Shatterer",        "Jahai Bluffs",         120,  75, nullptr, 0 },
    { 1263, "Palawadan",           "Domain of Istan",      120, 105, nullptr, 0 },
    { 1228, "Doppelganger",        "Elon Riverlands",      120, 115, nullptr, 0 },

    // ---- Living World S4 / Icebrood Saga metas — 2-hour cycle -----------
    { 1310, "Thunderhead Keep",    "Thunderhead Peaks",    120, 105, nullptr, 0 },
    { 1330, "Metal Concert",       "Grothmar Valley",      120, 100, nullptr, 0 },
    { 1343, "Drakkar",             "Bjora Marches",        120,  80, nullptr, 0 },

    // ---- End of Dragons metas — 2-hour cycle ----------------------------
    { 1438, "Kaineng Blackout",    "New Kaineng City",     120,   0, nullptr, 0 },
    { 1452, "Gang War",            "The Echovald Wilds",   120,  30, nullptr, 0 },
    { 1422, "Dragon's End",        "Dragon's End",         120,  60, nullptr, 0 },
    { 1442, "Aetherblade Assault", "Seitung Province",     120,  90, nullptr, 0 },

    // ---- Secrets of the Obscure metas — 2-hour cycle -------------------
    { 1517, "Defense of Amnytas",  "Amnytas",              120,   0, nullptr, 0 },
    { 1510, "Wizard's Tower",      "Skywatch Archipelago", 120,  60, nullptr, 0 },
};
static const int s_eventCount = (int)(sizeof(s_events) / sizeof(s_events[0]));

// City map IDs — players here get a "next event across all maps" display.
static const uint32_t s_cityMaps[] = {
     50,  // Lion's Arch
    139,  // Divinity's Reach
    218,  // Black Citadel
    335,  // Rata Sum
     91,  // The Grove
    179,  // Hoelbrak
};
static const int s_cityCount = (int)(sizeof(s_cityMaps) / sizeof(s_cityMaps[0]));

// ---------------------------------------------------------------------------

static int SecondsUntilNext(const EventEntry& e) {
    time_t now = time(nullptr);
    struct tm* utc = gmtime(&now);
    int secsIntoCurrent = utc->tm_hour * 3600 + utc->tm_min * 60 + utc->tm_sec;

    if (e.intervalMinutes > 0) {
        int blockSec          = e.intervalMinutes * 60;
        int secsIntoBlock     = secsIntoCurrent % blockSec;
        int eventStartInBlock = e.offsetMinutes * 60;
        return (eventStartInBlock - secsIntoBlock + blockSec) % blockSec;
    }

    // Irregular schedule: find the nearest upcoming time in the 24-hour day.
    const int daySec = 24 * 3600;
    int best = daySec;
    for (int i = 0; i < e.timesCount; i++) {
        int diff = (e.times[i] * 60 - secsIntoCurrent + daySec) % daySec;
        if (diff < best) best = diff;
    }
    return best;
}

EventCountdown Events_GetForMap(uint32_t mapId) {
    // Return the soonest event on this map (handles maps with multiple events).
    EventCountdown best = { nullptr, nullptr, INT_MAX, false };
    for (int i = 0; i < s_eventCount; i++) {
        if (s_events[i].mapId != mapId) continue;
        int sec = SecondsUntilNext(s_events[i]);
        if (!best.valid || sec < best.secondsUntil)
            best = { s_events[i].name, s_events[i].mapName, sec, true };
    }
    return best;
}

EventCountdown Events_GetNextAny() {
    EventCountdown best = { nullptr, nullptr, INT_MAX, false };
    for (int i = 0; i < s_eventCount; i++) {
        int sec = SecondsUntilNext(s_events[i]);
        if (sec < best.secondsUntil)
            best = { s_events[i].name, s_events[i].mapName, sec, true };
    }
    return best;
}

bool Events_IsCityMap(uint32_t mapId) {
    for (int i = 0; i < s_cityCount; i++)
        if (s_cityMaps[i] == mapId) return true;
    return false;
}
