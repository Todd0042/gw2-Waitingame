#include "nexus/Nexus.h"
#include "game.h"
#include "events.h"
#include "map_png.h"
#include <atomic>
#include <cstring>
#include <windows.h>

// ---------------------------------------------------------------------------
// Globals shared with render.cpp (via extern)
// ---------------------------------------------------------------------------
AddonAPI_t*           APIDefs       = nullptr;
bool                  g_windowOpen  = false;
std::atomic<uint32_t> g_currentMapId{ 0 };
char                  g_addonDir[MAX_PATH] = {};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void AddonLoad(AddonAPI_t* aAPIDefs);
void AddonUnload();
void OnRender();
void OnKeybind(const char* aIdentifier, bool aIsRelease);
void OnMumbleIdentityUpdated(void* aEventArgs);

// ---------------------------------------------------------------------------
// Nexus addon definition
// ---------------------------------------------------------------------------
static AddonDefinition_t s_addonDef = {};

extern "C" __declspec(dllexport)
AddonDefinition_t* GetAddonDef() {
    s_addonDef.Signature    = -556677;
    s_addonDef.APIVersion   = NEXUS_API_VERSION;
    s_addonDef.Name         = "Waitingame";
    s_addonDef.Version      = { 0, 1, 0, 0 };
    s_addonDef.Author       = "Todd0042";
    s_addonDef.Description  = "Shmup to pass the time waiting for GW2 events";
    s_addonDef.Load         = AddonLoad;
    s_addonDef.Unload       = AddonUnload;
    s_addonDef.Flags        = AF_None;
    s_addonDef.Provider     = UP_GitHub;
    s_addonDef.UpdateLink   = "https://github.com/Todd0042/gw2-Waitingame";
    return &s_addonDef;
}

// ---------------------------------------------------------------------------
// AddonLoad / AddonUnload
// ---------------------------------------------------------------------------

void AddonLoad(AddonAPI_t* aAPIDefs) {
    APIDefs = aAPIDefs;

    // Grab and create addon directory for score persistence
    const char* dir = APIDefs->Paths_GetAddonDirectory("waitingame");
    if (dir) {
        strncpy(g_addonDir, dir, sizeof(g_addonDir) - 1);
        CreateDirectoryA(g_addonDir, nullptr);
    }

    // Register render callback — actual ImGui/game init is deferred to first frame
    APIDefs->GUI_Register(RT_Render, OnRender);

    // Keybind to toggle the game window (player configures the key in Nexus settings)
    APIDefs->InputBinds_RegisterWithString("KB_WAITINGAME_TOGGLE", OnKeybind, "ALT+G");

    // Register icon textures from the embedded PNG byte array (normal + hover, same image)
    APIDefs->Textures_GetOrCreateFromMemory(
        "ICON_WAITINGAME",       (void*)map_png, (uint64_t)map_png_len);
    APIDefs->Textures_GetOrCreateFromMemory(
        "ICON_WAITINGAME_HOVER", (void*)map_png, (uint64_t)map_png_len);

    // Add icon to the Nexus quick-access bar
    APIDefs->QuickAccess_Add(
        "QA_WAITINGAME",
        "ICON_WAITINGAME",
        "ICON_WAITINGAME_HOVER",
        "KB_WAITINGAME_TOGGLE",
        "Waitingame"
    );

    // Listen for map/character changes
    APIDefs->Events_Subscribe("EV_MUMBLE_IDENTITY_UPDATED", OnMumbleIdentityUpdated);
}

void AddonUnload() {
    APIDefs->Events_Unsubscribe("EV_MUMBLE_IDENTITY_UPDATED", OnMumbleIdentityUpdated);
    APIDefs->InputBinds_Deregister("KB_WAITINGAME_TOGGLE");
    APIDefs->QuickAccess_Remove("QA_WAITINGAME");
    APIDefs->GUI_Deregister(OnRender);
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void OnKeybind(const char* aIdentifier, bool aIsRelease) {
    if (aIsRelease) return;
    if (strcmp(aIdentifier, "KB_WAITINGAME_TOGGLE") == 0)
        g_windowOpen = !g_windowOpen;
}

void OnMumbleIdentityUpdated(void* aEventArgs) {
    if (!aEventArgs) return;
    // Extract MapID directly from the raw pointer to avoid struct conflicts with Nexus.h.
    // Layout: Name[20] + Profession(4) + Specialization(4) + Race(4) = MapID at byte 32.
    uint32_t mapId = 0;
    memcpy(&mapId, static_cast<char*>(aEventArgs) + 32, sizeof(mapId));
    g_currentMapId.store(mapId, std::memory_order_relaxed);
}
