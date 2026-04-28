#include "imgui/imgui.h"
#include "nexus/Nexus.h"
#include "game.h"
#include "events.h"
#include <chrono>
#include <atomic>
#include <cstdio>
#include <algorithm>
#include <windows.h>

// Shared globals from addon.cpp
extern AddonAPI_t*            APIDefs;
extern bool                   g_windowOpen;
extern std::atomic<uint32_t>  g_currentMapId;
extern char                   g_addonDir[MAX_PATH];

// ---------------------------------------------------------------------------
// File I/O for high score
// ---------------------------------------------------------------------------
void SaveHighScore() {
    if (g_game.highScore <= 0 || g_addonDir[0] == '\0') return;

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s\\score.dat", g_addonDir);

    FILE* f = fopen(path, "wb");
    if (f) {
        fwrite(&g_game.highScore, sizeof(int), 1, f);
        fclose(f);
    }
}

static void LoadHighScore() {
    if (g_addonDir[0] == '\0') return;

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s\\score.dat", g_addonDir);

    FILE* f = fopen(path, "rb");
    if (f) {
        fread(&g_game.highScore, sizeof(int), 1, f);
        fclose(f);
    }
}

// ---------------------------------------------------------------------------
// Frame-local state
// ---------------------------------------------------------------------------

static bool        s_initialized     = false;
static bool        s_triggerWarning  = false;
static bool        s_popupIsOpen     = false;
static const char* s_warnEventName   = "the event";
static const char* s_warnMapName     = "";
static const char* s_closeEventName  = "the event";

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

static void DrawBackground(ImDrawList* dl, ImVec2 co) {
    dl->AddRectFilled(co, { co.x + CANVAS_W, co.y + CANVAS_H },
                      IM_COL32(8, 8, 20, 255));

    for (int i = 0; i < NUM_STARS; i++) {
        const Star& s = g_game.stars[i];
        float t = s.speed / 90.0f;
        auto  b = (uint8_t)(70 + t * 155);
        auto  c = (uint8_t)std::min(255.0f, 70 + t * 170);
        dl->AddRectFilled(
            { co.x + s.x,          co.y + s.y },
            { co.x + s.x + s.size, co.y + s.y + s.size },
            IM_COL32(b, b, c, 255)
        );
    }
}

static void DamagePlayerFromLaser() {
    Player& pl = g_game.player;
    if (pl.invTimer > 0.0f) return;

    pl.lives--;
    pl.invTimer = 2.0f;
    Game_SpawnParticles(pl.pos, 8, 85.0f, MakeColor(80, 200, 255));
    if (pl.lives <= 0)
        g_game.state = GameState::GameOver;
}

static bool PlayerIntersectsRect(float px, float py, float rx1, float ry1, float rx2, float ry2) {
    return (px >= rx1 && px <= rx2 && py >= ry1 && py <= ry2);
}

static void DrawBossLasersAndDamage(ImDrawList* dl, ImVec2 co, const Enemy& e) {
    if (!e.bossLaserActive) return;

    float cx = co.x + e.pos.x;
    float cy = co.y + e.pos.y;
    float bottom = co.y + CANVAS_H;

    float px = co.x + g_game.player.pos.x;
    float py = co.y + g_game.player.pos.y;

    if (e.type == 4) {
        float half = CANVAS_W * 0.10f;
        float x1 = cx - half;
        float x2 = cx + half;
        float y1 = cy + 12.0f;
        float y2 = bottom;

        dl->AddRectFilled({ x1, y1 }, { x2, y2 }, IM_COL32(255, 255, 160, 180));
        dl->AddRectFilled({ cx - half * 0.6f, y1 }, { cx + half * 0.6f, y2 },
                          IM_COL32(255, 255, 220, 220));

        if (PlayerIntersectsRect(px, py, x1, y1, x2, y2))
            DamagePlayerFromLaser();
    }

    if (e.type == 5) {
        float half = CANVAS_W * 0.05f;
        float y1 = cy + 12.0f;
        float y2 = bottom;

        float cx1 = cx - half;
        float cx2 = cx + half;

        dl->AddRectFilled({ cx1, y1 }, { cx2, y2 }, IM_COL32(160, 255, 255, 180));
        dl->AddRectFilled({ cx - half * 0.6f, y1 }, { cx + half * 0.6f, y2 },
                          IM_COL32(220, 255, 255, 220));

        if (PlayerIntersectsRect(px, py, cx1, y1, cx2, y2))
            DamagePlayerFromLaser();

        float lx1 = cx - 10.0f - half;
        float lx2 = cx - 10.0f + half;
        float lx3 = co.x + CANVAS_W * 0.15f - half;
        float lx4 = co.x + CANVAS_W * 0.15f + half;

        dl->AddQuadFilled(
            { lx1, y1 }, { lx2, y1 }, { lx4, y2 }, { lx3, y2 },
            IM_COL32(160, 255, 255, 160)
        );

        float rx1 = cx + 10.0f - half;
        float rx2 = cx + 10.0f + half;
        float rx3 = co.x + CANVAS_W * 0.85f - half;
        float rx4 = co.x + CANVAS_W * 0.85f + half;

        dl->AddQuadFilled(
            { rx1, y1 }, { rx2, y1 }, { rx4, y2 }, { rx3, y2 },
            IM_COL32(160, 255, 255, 160)
        );

        if (PlayerIntersectsRect(px, py, lx1, y1, lx4, y2) ||
            PlayerIntersectsRect(px, py, rx1, y1, rx4, y2))
            DamagePlayerFromLaser();
    }
}

static void DrawPlayer(ImDrawList* dl, ImVec2 co) {
    const Player& pl = g_game.player;
    if (pl.invTimer > 0.0f && (int)(pl.invTimer * 8) % 2 == 0) return;

    float cx = co.x + pl.pos.x;
    float cy = co.y + pl.pos.y;

    dl->AddTriangleFilled(
        { cx,        cy - 14.0f },
        { cx - 10.0f, cy + 14.0f },
        { cx + 10.0f, cy + 14.0f },
        IM_COL32(50, 220, 100, 255)
    );
    dl->AddRectFilled(
        { cx - 5.0f, cy + 12.0f }, { cx + 5.0f, cy + 18.0f },
        IM_COL32(80, 180, 255, 200)
    );
}

static void DrawEnemies(ImDrawList* dl, ImVec2 co) {
    static const uint32_t colors[] = {
        IM_COL32(220,  50,  50, 255),
        IM_COL32(220, 140,  50, 255),
        IM_COL32(160,  50, 220, 255),
        IM_COL32(220,  50, 160, 255),
        IM_COL32(255, 255,  80, 255),
        IM_COL32(120, 255, 255, 255),
    };

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy& e = g_game.enemies[i];
        if (!e.active) continue;

        float cx = co.x + e.pos.x;
        float cy = co.y + e.pos.y;
        uint32_t c = colors[e.type];

        switch (e.type) {
            case 0:
                dl->AddTriangleFilled(
                    { cx,        cy +  9.0f },
                    { cx -  9.0f, cy -  9.0f },
                    { cx +  9.0f, cy -  9.0f },
                    c
                );
                break;

            case 1:
                dl->AddQuadFilled(
                    { cx,        cy - 10.0f },
                    { cx + 10.0f, cy        },
                    { cx,        cy + 10.0f },
                    { cx - 10.0f, cy        },
                    c
                );
                break;

            case 2:
                dl->AddRectFilled({ cx - 11.0f, cy - 11.0f }, { cx + 11.0f, cy + 11.0f }, c);
                dl->AddRectFilled({ cx - 14.0f, cy -  3.0f }, { cx - 11.0f, cy +  3.0f },
                                  IM_COL32(200, 80, 255, 255));
                dl->AddRectFilled({ cx + 11.0f, cy -  3.0f }, { cx + 14.0f, cy +  3.0f },
                                  IM_COL32(200, 80, 255, 255));
                if (e.hp >= 2)
                    dl->AddRectFilled({ cx - 7.0f, cy - 7.0f }, { cx + 7.0f, cy + 7.0f },
                                      IM_COL32(220, 180, 50, 160));
                break;

            case 3:
                dl->AddTriangleFilled(
                    { cx,         cy + 7.0f },
                    { cx -  7.0f, cy - 7.0f },
                    { cx +  7.0f, cy - 7.0f },
                    c
                );
                break;

            case 4:
                dl->AddRectFilled(
                    { cx - 34.0f, cy - 12.0f },
                    { cx + 34.0f, cy + 12.0f },
                    c
                );
                dl->AddRectFilled(
                    { cx - 18.0f, cy - 6.0f },
                    { cx + 18.0f, cy + 6.0f },
                    IM_COL32(255, 255, 200, 220)
                );
                DrawBossLasersAndDamage(dl, co, e);
                break;

            case 5:
                dl->AddRectFilled(
                    { cx - 34.0f, cy - 12.0f },
                    { cx + 34.0f, cy + 12.0f },
                    c
                );
                dl->AddRectFilled(
                    { cx - 18.0f, cy - 6.0f },
                    { cx + 18.0f, cy + 6.0f },
                    IM_COL32(200, 255, 255, 220)
                );
                DrawBossLasersAndDamage(dl, co, e);
                break;
        }
    }
}
static void DrawBullets(ImDrawList* dl, ImVec2 co) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet& b = g_game.bullets[i];
        if (!b.active) continue;
        float bx = co.x + b.pos.x;
        float by = co.y + b.pos.y;
        if (b.isEnemy)
            dl->AddCircleFilled({ bx, by }, 4.0f, IM_COL32(255, 100, 30, 255), 8);
        else
            dl->AddRectFilled({ bx - 2.0f, by - 6.0f }, { bx + 2.0f, by + 6.0f },
                              IM_COL32(255, 240, 50, 255));
    }
}

static void DrawParticles(ImDrawList* dl, ImVec2 co) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle& p = g_game.particles[i];
        if (!p.active) continue;
        auto a    = (uint8_t)((p.life / p.maxLife) * 255.0f);
        uint32_t c = (p.color & 0x00FFFFFFu) | ((uint32_t)a << 24);
        float px = co.x + p.pos.x;
        float py = co.y + p.pos.y;
        dl->AddRectFilled({ px - 2.0f, py - 2.0f }, { px + 2.0f, py + 2.0f }, c);
    }
}

static void DrawHUD(ImDrawList* dl, ImVec2 ho, const EventCountdown& ec) {
    dl->AddRectFilled(ho, { ho.x + CANVAS_W, ho.y + HUD_H },
                      IM_COL32(5, 5, 15, 230));
    dl->AddLine({ ho.x, ho.y + HUD_H - 1.0f },
                { ho.x + CANVAS_W, ho.y + HUD_H - 1.0f },
                IM_COL32(40, 80, 180, 120));

    ImGui::SetCursorScreenPos({ ho.x + 8.0f, ho.y + 4.0f });
    ImGui::TextColored({ 0.9f, 0.9f, 0.3f, 1.0f }, "SCORE  %d", g_game.score);

    ImGui::SetCursorScreenPos({ ho.x + 8.0f, ho.y + 22.0f });
    ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 1.0f }, "BEST   %d", g_game.highScore);

    for (int i = 0; i < g_game.player.lives; i++) {
        float lx = ho.x + CANVAS_W - 20.0f - i * 18.0f;
        float ly = ho.y + HUD_H * 0.5f;
        dl->AddTriangleFilled(
            { lx + 5.0f, ly - 7.0f },
            { lx,        ly + 7.0f },
            { lx + 10.0f, ly + 7.0f },
            IM_COL32(50, 200, 80, 200)
        );
    }

    if (g_game.state == GameState::Playing || g_game.state == GameState::GameOver) {
        float bombBase = ho.x + CANVAS_W - 20.0f - (float)PLAYER_LIVES * 18.0f - 18.0f;
        float by = ho.y + HUD_H * 0.5f;
        for (int i = 0; i < g_game.bombs; i++) {
            float bx = bombBase - i * 14.0f;
            dl->AddQuadFilled(
                { bx + 5.0f, by - 5.0f },
                { bx + 10.0f, by        },
                { bx + 5.0f, by + 5.0f },
                { bx,         by        },
                IM_COL32(80, 200, 255, 200)
            );
        }
    }

    if (ec.valid) {
        int mins = ec.secondsUntil / 60;
        int secs = ec.secondsUntil % 60;
        float r = 0.9f, g2 = 0.9f, b2 = 0.3f;
        if (ec.secondsUntil <= 60) { r = 1.0f; g2 = 0.3f; b2 = 0.2f; }

        char buf[64];
        snprintf(buf, sizeof(buf), "%.18s %02d:%02d", ec.name, mins, secs);

        ImVec2 tsz = ImGui::CalcTextSize(buf);
        ImGui::SetCursorScreenPos({ ho.x + CANVAS_W - tsz.x - 24.0f, ho.y + 4.0f });
        ImGui::TextColored({ r, g2, b2, 1.0f }, "%s", buf);
    }
}

static void CenteredText(ImVec2 co, float y, const char* text) {
    float tw = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorScreenPos({ co.x + (CANVAS_W - tw) * 0.5f, y });
    ImGui::TextUnformatted(text);
}

static void DrawMenu(ImDrawList* dl, ImVec2 co, const EventCountdown& ec) {
    dl->AddRectFilled(co, { co.x + CANVAS_W, co.y + CANVAS_H },
                      IM_COL32(0, 0, 0, 160));

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
    CenteredText(co, co.y + 130.0f, "Waitingame");
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    CenteredText(co, co.y + 155.0f, "A GW2 Shmup");
    ImGui::PopStyleColor();

    if (ec.valid) {
        int mins = ec.secondsUntil / 60;
        int secs = ec.secondsUntil % 60;
        char timeBuf[64];
        snprintf(timeBuf, sizeof(timeBuf), "%s in %d:%02d", ec.name, mins, secs);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.8f, 0.2f, 1.0f));
        CenteredText(co, co.y + 200.0f, timeBuf);
        ImGui::PopStyleColor();
        char mapBuf[64];
        snprintf(mapBuf, sizeof(mapBuf), "on %s", ec.mapName);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        CenteredText(co, co.y + 218.0f, mapBuf);
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        CenteredText(co, co.y + 200.0f, "Free play - no event tracked");
        ImGui::PopStyleColor();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 0.8f));
    CenteredText(co, co.y + 310.0f, "Arrows: move   LShift: shoot   CapsLock: bomb");
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.3f, 1.0f));
    CenteredText(co, co.y + CANVAS_H - 70.0f, "Press ENTER or SPACE to play");
    ImGui::PopStyleColor();

    if (g_game.highScore > 0) {
        char bestBuf[32];
        snprintf(bestBuf, sizeof(bestBuf), "Best: %d", g_game.highScore);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        CenteredText(co, co.y + CANVAS_H - 50.0f, bestBuf);
        ImGui::PopStyleColor();
    }
}

static void DrawGameOver(ImDrawList* dl, ImVec2 co) {
    dl->AddRectFilled(co, { co.x + CANVAS_W, co.y + CANVAS_H },
                      IM_COL32(0, 0, 0, 180));

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    CenteredText(co, co.y + 160.0f, "GAME OVER");
    ImGui::PopStyleColor();

    char scoreBuf[64];
    snprintf(scoreBuf, sizeof(scoreBuf), "Score: %d", g_game.score);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.3f, 1.0f));
    CenteredText(co, co.y + 190.0f, scoreBuf);
    ImGui::PopStyleColor();

    if (g_game.score >= g_game.highScore && g_game.score > 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.1f, 1.0f));
        CenteredText(co, co.y + 210.0f, "NEW BEST!");
        ImGui::PopStyleColor();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.3f, 1.0f));
    CenteredText(co, co.y + CANVAS_H - 70.0f, "Press ENTER or SPACE to play again");
    ImGui::PopStyleColor();
}

static void DrawEventClose(ImVec2 co) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(co, { co.x + CANVAS_W, co.y + CANVAS_H },
                      IM_COL32(0, 0, 0, 200));

    char line1[128];
    snprintf(line1, sizeof(line1), "Time for %s!", s_closeEventName);
    float tw1 = ImGui::CalcTextSize(line1).x;
    ImGui::SetCursorScreenPos({ co.x + (CANVAS_W - tw1) * 0.5f, co.y + CANVAS_H * 0.5f - 18.0f });
    ImGui::TextColored({ 1.0f, 0.85f, 0.1f, 1.0f }, "%s", line1);

    const char* line2 = "Good luck, Commander!";
    float tw2 = ImGui::CalcTextSize(line2).x;
    ImGui::SetCursorScreenPos({ co.x + (CANVAS_W - tw2) * 0.5f, co.y + CANVAS_H * 0.5f + 6.0f });
    ImGui::TextColored({ 0.8f, 0.8f, 0.8f, 1.0f }, "%s", line2);
}

// ---------------------------------------------------------------------------
// Main render callback
// ---------------------------------------------------------------------------

void OnRender() {
    if (!s_initialized) {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(APIDefs->ImguiContext));
        ImGui::SetAllocatorFunctions(
            reinterpret_cast<void*(*)(size_t, void*)>(APIDefs->ImguiMalloc),
            reinterpret_cast<void (*)(void*, void*)>(APIDefs->ImguiFree),
            nullptr
        );
        Game_Init();
        LoadHighScore();
        s_initialized = true;
    }

    if (!g_windowOpen) return;

    static auto s_last = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float dt = std::min(std::chrono::duration<float>(now - s_last).count(), 0.05f);
    s_last = now;

    uint32_t mapId = g_currentMapId.load(std::memory_order_relaxed);
    EventCountdown ec = Events_GetForMap(mapId);
    bool onEventMap = ec.valid;
    if (!ec.valid)
        ec = Events_GetNextAny();

    if (g_game.state == GameState::Playing) {
        if (onEventMap && ec.secondsUntil <= 0) {
            s_closeEventName = ec.name;
            SaveHighScore();
            g_game.state = GameState::EventClose;
        } else if (onEventMap && ec.secondsUntil <= 60 && !g_game.warningShown) {
            g_game.warningShown = true;
            s_warnEventName     = ec.name;
            s_warnMapName       = ec.mapName;
            s_triggerWarning    = true;
        }
    }

    GameInput input = {};
    ImGuiIO& io = ImGui::GetIO();

    static GameState s_prevState     = GameState::Menu;
    static bool      s_confirmBlocked = false;
    if (g_game.state != s_prevState) {
        if (g_game.state == GameState::GameOver)
            s_confirmBlocked = true;
        s_prevState = g_game.state;
    }
    if (s_confirmBlocked && !io.KeysDown[VK_RETURN] && !io.KeysDown[VK_SPACE])
        s_confirmBlocked = false;

    if (g_game.state == GameState::Playing && !s_popupIsOpen) {
        if (io.KeysDown[VK_LEFT])  input.dx -= 1.0f;
        if (io.KeysDown[VK_RIGHT]) input.dx += 1.0f;
        if (io.KeysDown[VK_UP])    input.dy -= 1.0f;
        if (io.KeysDown[VK_DOWN])  input.dy += 1.0f;
        input.fire = io.KeyShift;
        input.bomb = ImGui::IsKeyPressed(VK_CAPITAL, false);
    }
    if (!s_confirmBlocked && (g_game.state == GameState::Menu || g_game.state == GameState::GameOver))
        input.confirm = ImGui::IsKeyPressed(VK_RETURN, false) || ImGui::IsKeyPressed(VK_SPACE, false);

    if (!s_popupIsOpen)
        Game_Update(dt, input);

    bool wasOpen = g_windowOpen;
    if (g_game.state == GameState::EventClose && g_game.closeTimer <= 0.0f)
        g_windowOpen = false;

    if (!g_windowOpen) {
        if (wasOpen) SaveHighScore();
        return;
    }

    ImGui::SetNextWindowSize({ WIN_W, WIN_H }, ImGuiCond_Always);
    ImGuiWindowFlags wflags =
        ImGuiWindowFlags_NoResize      |
        ImGuiWindowFlags_NoScrollbar   |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoNav         |
        ImGuiWindowFlags_NoCollapse;

    if (!ImGui::Begin("Waitingame##wg", &g_windowOpen, wflags)) {
        ImGui::End();
        return;
    }

    ImVec2 cs = ImGui::GetCursorScreenPos();
    ImVec2 hudOrigin    = cs;
    ImVec2 canvasOrigin = { cs.x, cs.y + HUD_H };

    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->PushClipRect(canvasOrigin,
                     { canvasOrigin.x + CANVAS_W, canvasOrigin.y + CANVAS_H },
                     true);
    DrawBackground(dl, canvasOrigin);
    if (g_game.state != GameState::Menu)
        DrawPlayer(dl, canvasOrigin);
    DrawEnemies(dl, canvasOrigin);
    DrawBullets(dl, canvasOrigin);
    DrawParticles(dl, canvasOrigin);
    dl->PopClipRect();

    if (g_game.state == GameState::Menu)
        DrawMenu(dl, canvasOrigin, ec);
    else if (g_game.state == GameState::GameOver)
        DrawGameOver(dl, canvasOrigin);
    else if (g_game.state == GameState::EventClose)
        DrawEventClose(canvasOrigin);

    DrawHUD(dl, hudOrigin, ec);

    ImGui::SetCursorScreenPos({ cs.x, cs.y + HUD_H + CANVAS_H - 1.0f });
    ImGui::Dummy({ 1.0f, 1.0f });

    ImGui::SetCursorScreenPos({ cs.x, cs.y + HUD_H + CANVAS_H + 4.0f });
    ImGui::SetNextItemWidth(CANVAS_W);
    static char s_suppressBuf[] = "click here to suppress keystrokes in-game";
    ImGui::InputText("##suppress", s_suppressBuf, sizeof(s_suppressBuf),
                     ImGuiInputTextFlags_ReadOnly);

    if (s_triggerWarning) {
        ImGui::SetNextWindowPos(
            { cs.x + CANVAS_W * 0.5f, cs.y + HUD_H + CANVAS_H * 0.5f },
            ImGuiCond_Always, { 0.5f, 0.5f }
        );
        ImGui::OpenPopup("##wg_warn");
        s_triggerWarning = false;
        s_popupIsOpen    = true;
    }

    if (ImGui::BeginPopupModal("##wg_warn", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoTitleBar)) {

        ImGui::TextColored({ 1.0f, 0.8f, 0.15f, 1.0f }, "%s", s_warnEventName);
        ImGui::Text("on %s starts in about 1 minute!", s_warnMapName);
        ImGui::Spacing();

        if (ImGui::Button("Keep Playing", { 130.0f, 0 })) {
            s_popupIsOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop Now", { 100.0f, 0 })) {
            s_popupIsOpen = false;
            SaveHighScore();
            g_windowOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Save if window X button was clicked this frame
    if (wasOpen && !g_windowOpen)
        SaveHighScore();

    ImGui::End(); // <-- closes the main "Waitingame" window
} // <-- closes OnRender()
