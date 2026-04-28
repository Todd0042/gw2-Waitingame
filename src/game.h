#pragma once
#include <cstdint>

// Canvas and window sizing (pixels)
static constexpr float CANVAS_W        = 340.0f;
static constexpr float CANVAS_H        = 460.0f;
static constexpr float HUD_H           = 45.0f;
static constexpr float SUPPRESS_BAR_H  = 30.0f;
// Outer window: canvas + padding (8px each side) + title bar (~19px)
static constexpr float WIN_W    = CANVAS_W + 16.0f;
static constexpr float WIN_H    = CANVAS_H + HUD_H + SUPPRESS_BAR_H + 35.0f;

static constexpr int MAX_BULLETS   = 256;
static constexpr int MAX_ENEMIES   = 96;
static constexpr int MAX_PARTICLES = 192;
static constexpr int NUM_STARS     = 60;
static constexpr int PLAYER_LIVES  = 3;
static constexpr int PLAYER_BOMBS  = 2;

enum class GameState { Menu, Playing, GameOver, EventClose };

struct Vec2 { float x, y; };

// Packs RGBA in the same byte order as ImGui's IM_COL32 so render.cpp
// can use particle colors directly without conversion.
static inline uint32_t MakeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
}

struct Star {
    float x, y, speed, size;
};

struct Player {
    Vec2  pos;
    float invTimer;   // invincibility seconds remaining after hit
    int   lives;
    float fireTimer;  // seconds until next shot allowed
};

struct Bullet {
    Vec2 pos;
    Vec2 vel;
    bool isEnemy;
    bool active;
};

// Boss support
struct Enemy;
using BossLaserFunc = void(*)(Enemy&);

enum class BossState {
    None,
    MoveToPosition,
    Pause,
    FiringLaser
};

struct Enemy {
    Vec2  pos;
    Vec2  vel;
    float baseX;       // original spawn X for sinusoidal type
    float phase;       // sine phase accumulator (type 1)
    float shootTimer;  // seconds until next shot (type 2)
    int   type;        // 0=Drone 1=Weaver 2=Gunner 3=Rusher 4=Boss1 5=Boss2
    int   hp;
    bool  active;

    // Boss-specific fields (used when type >= 4)
    BossState     bossState;
    BossState     bossNextState;
    float         bossTimer;
    float         bossPauseDuration;
    float         bossLaserDuration;
    bool          bossLaserActive;
    float         bossTargetX;
    BossLaserFunc bossLaserFunc;
};

struct Particle {
    Vec2     pos;
    Vec2     vel;
    float    life;
    float    maxLife;
    uint32_t color;   // MakeColor() format, alpha overwritten at draw time
    bool     active;
};

struct GameInput {
    float dx;       // -1/0/1 horizontal
    float dy;       // -1/0/1 vertical
    bool  fire;
    bool  bomb;
    bool  confirm;  // start / restart
};

struct GameData {
    GameState state;
    Player    player;
    Bullet    bullets[MAX_BULLETS];
    Enemy     enemies[MAX_ENEMIES];
    Particle  particles[MAX_PARTICLES];
    Star      stars[NUM_STARS];
    int       score;
    int       highScore;
    int       bombs;
    float     elapsed;      // seconds since current game started
    float     spawnTimer;   // seconds until next enemy spawn
    bool      warningShown; // 1-minute warning popup already shown this game
    float     closeTimer;   // EventClose countdown (seconds)

    // Boss / progression control
    bool      bossActive;
    bool      boss1Spawned;
    bool      boss2Spawned;
    bool      progressionPaused;
};

extern GameData g_game;

void Game_Init();
void Game_Reset();
void Game_Update(float dt, const GameInput& input);
void Game_SpawnParticles(Vec2 pos, int count, float speed, uint32_t color);
