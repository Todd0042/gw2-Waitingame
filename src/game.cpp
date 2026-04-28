#include "game.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <algorithm>

void SaveHighScore();


GameData g_game = {};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static float Randf(float lo, float hi) {
    return lo + (float)rand() / (float)RAND_MAX * (hi - lo);
}

static bool AABBOverlap(Vec2 a, float ahw, float ahh,
                        Vec2 b, float bhw, float bhh) {
    return fabsf(a.x - b.x) < ahw + bhw
        && fabsf(a.y - b.y) < ahh + bhh;
}

static int FreeBullet() {
    for (int i = 0; i < MAX_BULLETS; i++) if (!g_game.bullets[i].active) return i;
    return -1;
}
static int FreeEnemy() {
    for (int i = 0; i < MAX_ENEMIES; i++) if (!g_game.enemies[i].active) return i;
    return -1;
}
static int FreeParticle() {
    for (int i = 0; i < MAX_PARTICLES; i++) if (!g_game.particles[i].active) return i;
    return -1;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Game_SpawnParticles(Vec2 pos, int count, float speed, uint32_t color) {
    for (int i = 0; i < count; i++) {
        int idx = FreeParticle();
        if (idx < 0) break;
        Particle& p = g_game.particles[idx];
        float angle = Randf(0.0f, 6.2832f);
        float spd   = Randf(speed * 0.5f, speed);
        p.pos     = pos;
        p.vel     = { cosf(angle) * spd, sinf(angle) * spd };
        p.life    = Randf(0.2f, 0.45f);
        p.maxLife = p.life;
        p.color   = color;
        p.active  = true;
    }
}

void Game_Init() {
    memset(&g_game, 0, sizeof(g_game));
    g_game.state = GameState::Menu;
    for (int i = 0; i < NUM_STARS; i++) {
        g_game.stars[i] = {
            Randf(0, CANVAS_W), Randf(0, CANVAS_H),
            Randf(15.0f, 90.0f), Randf(1.0f, 2.5f)
        };
    }
}

void Game_Reset() {
    int   hs = g_game.highScore;
    Star  stars[NUM_STARS];
    memcpy(stars, g_game.stars, sizeof(stars));

    memset(&g_game, 0, sizeof(g_game));

    g_game.highScore   = hs;
    memcpy(g_game.stars, stars, sizeof(stars));

    g_game.state            = GameState::Playing;
    g_game.player.pos       = { CANVAS_W * 0.5f, CANVAS_H - 60.0f };
    g_game.player.lives     = PLAYER_LIVES;
    g_game.bombs            = PLAYER_BOMBS;
    g_game.spawnTimer       = 1.0f;
    g_game.closeTimer       = 3.0f;
    g_game.bossActive       = false;
    g_game.boss1Spawned     = false;
    g_game.boss2Spawned     = false;
    g_game.progressionPaused = false;
}

// ---------------------------------------------------------------------------
// Internal update helpers
// ---------------------------------------------------------------------------

// Difficulty ramp: spawn interval shrinks from 1.5s toward 0.25s over ~10 min.
static float SpawnInterval() {
    return std::max(0.25f, 1.5f - g_game.elapsed * 0.0020f);
}
// Speed multiplier: starts at 1.25, ramps a bit faster.
static float SpeedMult() {
    return 1.25f + g_game.elapsed * 0.002f;
}

// Per-type combat stats (half-extents for AABB, points)
// 0=Drone 1=Weaver 2=Gunner 3=Rusher 4=Boss1 5=Boss2
static const float s_eHW[]  = {
    9.0f, 9.0f, 11.0f, 7.0f,
    34.0f, 34.0f
};
static const float s_eHH[]  = {
    9.0f, 9.0f, 11.0f, 7.0f,
    20.0f, 20.0f
};
static const int   s_ePts[] = {
    10,   15,   25,   20,
    200,  300
};

// Particle explosion colors per enemy type (matches render colors)
static const uint32_t s_eColor[] = {
    MakeColor(220,  50,  50),   // Drone   - red
    MakeColor(220, 140,  50),   // Weaver  - orange
    MakeColor(160,  50, 220),   // Gunner  - purple
    MakeColor(220,  50, 160),   // Rusher  - pink
    MakeColor(255, 255,  80),   // Boss1   - yellow
    MakeColor(120, 255, 255),   // Boss2   - cyan
};

// ---------------------------------------------------------------------------
// Boss laser behaviors (logic-only; render.cpp can inspect boss fields)
// ---------------------------------------------------------------------------

static void BossLaser_Single(Enemy& e) {
    // Single vertical beam straight down from boss.
    (void)e;
}

static void BossLaser_Triple(Enemy& e) {
    // Triple beam: down, down-left, down-right from boss.
    (void)e;
}

// ---------------------------------------------------------------------------
// Enemy / boss spawning
// ---------------------------------------------------------------------------

static void SpawnEnemy() {
    int idx = FreeEnemy();
    if (idx < 0) return;
    Enemy& e = g_game.enemies[idx];
    memset(&e, 0, sizeof(e));
    e.active = true;
    e.pos    = { Randf(15.0f, CANVAS_W - 15.0f), -20.0f };
    e.baseX  = e.pos.x;

    float sm = SpeedMult();
    int maxType = (g_game.elapsed > 150.0f) ? 3 :
                  (g_game.elapsed >  90.0f) ? 2 :
                  (g_game.elapsed >  45.0f) ? 1 : 0;
    e.type = rand() % (maxType + 1);

    switch (e.type) {
        case 0:
            e.vel = { 0, 80.0f * sm };  e.hp = 1;
            break;
        case 1:
            e.vel   = { 0, 65.0f * sm };
            e.hp    = 1;
            e.phase = Randf(0.0f, 6.28f);
            break;
        case 2:
            e.vel        = { 0, 45.0f * sm };
            e.hp         = 2;
            e.shootTimer = Randf(1.5f, 3.0f);
            break;
        case 3:
            e.vel = { (rand() % 2 ? 1.0f : -1.0f) * 75.0f * sm, 125.0f * sm };
            e.hp  = 1;
            break;
    }
}

static void SpawnBossCommon(Enemy& e) {
    e.active            = true;
    e.baseX             = e.pos.x;
    e.phase             = 0.0f;
    e.shootTimer        = 0.0f;
    e.bossState         = BossState::MoveToPosition;
    e.bossNextState     = BossState::Pause;
    e.bossTimer         = 0.0f;
    e.bossLaserActive   = false;
    e.bossTargetX       = Randf(CANVAS_W * 0.1f, CANVAS_W * 0.9f);
}

static void SpawnBoss1() {
    int idx = FreeEnemy();
    if (idx < 0) return;
    Enemy& e = g_game.enemies[idx];
    memset(&e, 0, sizeof(e));

    e.type              = 4;
    e.hp                = 40;
    e.pos               = { CANVAS_W * 0.5f, -80.0f };
    e.vel               = { 0.0f, 80.0f };
    e.bossPauseDuration = 3.0f;
    e.bossLaserDuration = 1.5f;
    e.bossLaserFunc     = BossLaser_Single;

    SpawnBossCommon(e);
}

static void SpawnBoss2() {
    int idx = FreeEnemy();
    if (idx < 0) return;
    Enemy& e = g_game.enemies[idx];
    memset(&e, 0, sizeof(e));

    e.type              = 5;
    e.hp                = 60;
    e.pos               = { CANVAS_W * 0.5f, -80.0f };
    e.vel               = { 0.0f, 80.0f };
    e.bossPauseDuration = 3.0f;
    e.bossLaserDuration = 1.5f;
    e.bossLaserFunc     = BossLaser_Triple;

    SpawnBossCommon(e);
}

// ---------------------------------------------------------------------------
// Collisions
// ---------------------------------------------------------------------------

static void HandleCollisions() {
    Player& pl = g_game.player;
    static constexpr float PHW = 8.0f, PHH = 11.0f;

    // Player bullets vs enemies
    for (int bi = 0; bi < MAX_BULLETS; bi++) {
        Bullet& b = g_game.bullets[bi];
        if (!b.active || b.isEnemy) continue;
        for (int ei = 0; ei < MAX_ENEMIES; ei++) {
            Enemy& e = g_game.enemies[ei];
            if (!e.active) continue;
            if (AABBOverlap(b.pos, 2.0f, 6.0f, e.pos, s_eHW[e.type], s_eHH[e.type])) {
                b.active = false;
                e.hp--;
                if (e.hp <= 0) {
                    e.active = false;
                    g_game.score += s_ePts[e.type];
                    if (g_game.score > g_game.highScore)
                        g_game.highScore = g_game.score;
                    Game_SpawnParticles(e.pos, 6 + e.type, 65.0f, s_eColor[e.type]);
                }
                break;
            }
        }
    }

    // Skip remaining if player is invincible
    if (pl.invTimer > 0) return;

    // Enemy bullets vs player
    for (int bi = 0; bi < MAX_BULLETS; bi++) {
        Bullet& b = g_game.bullets[bi];
        if (!b.active || !b.isEnemy) continue;

        if (AABBOverlap(b.pos, 3.0f, 3.0f, pl.pos, PHW, PHH)) {
            b.active = false;
            pl.lives--;
            pl.invTimer = 2.0f;
            Game_SpawnParticles(pl.pos, 8, 85.0f, MakeColor(80, 200, 255));

            if (pl.lives <= 0) {
                if (g_game.score > g_game.highScore)
                    g_game.highScore = g_game.score;
                SaveHighScore();
                g_game.state = GameState::GameOver;
            }

            return;
        }
    }

    // Enemy contact vs player
    for (int ei = 0; ei < MAX_ENEMIES; ei++) {
        Enemy& e = g_game.enemies[ei];
        if (!e.active) continue;

        if (AABBOverlap(e.pos, s_eHW[e.type], s_eHH[e.type], pl.pos, PHW, PHH)) {
            e.active = false;
            pl.lives--;
            pl.invTimer = 2.0f;
            Game_SpawnParticles(pl.pos, 8, 85.0f, MakeColor(80, 200, 255));

            if (pl.lives <= 0) {
                if (g_game.score > g_game.highScore)
                    g_game.highScore = g_game.score;
                SaveHighScore();
                g_game.state = GameState::GameOver;
            }

            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Boss enemy update (called from UpdatePlaying when bossActive)
// ---------------------------------------------------------------------------

static void UpdateBossEnemies(float dt) {
    bool bossAlive = false;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy& e = g_game.enemies[i];
        if (!e.active || e.type < 4) continue;
        bossAlive = true;

        // Boss death resets progression
        if (e.hp <= 0) {
            e.active = false;
            g_game.bossActive = false;
            g_game.progressionPaused = false;
            continue;
        }

        // Move down into arena first
        if (e.pos.y < CANVAS_H * 0.10f) {
            e.pos.y += e.vel.y * dt;
            if (e.pos.y >= CANVAS_H * 0.10f) {
                e.pos.y       = CANVAS_H * 0.10f;
                e.bossState   = BossState::MoveToPosition;
                e.bossTargetX = Randf(CANVAS_W * 0.1f, CANVAS_W * 0.9f);
            }
        } else {
            switch (e.bossState) {
                case BossState::MoveToPosition: {
                    float dx = e.bossTargetX - e.pos.x;
                    e.pos.x += dx * dt * 2.0f;
                    if (fabsf(dx) < 2.0f) {
                        e.bossState     = BossState::Pause;
                        e.bossTimer     = e.bossPauseDuration;
                        e.bossNextState = BossState::FiringLaser;
                    }
                } break;

                case BossState::Pause:
                    e.bossTimer -= dt;
                    if (e.bossTimer <= 0.0f) {
                        e.bossState = e.bossNextState;
                    }
                    break;

                case BossState::FiringLaser:
                    if (!e.bossLaserActive) {
                        e.bossLaserActive = true;
                        e.bossTimer       = e.bossLaserDuration;
                    }
                    if (e.bossLaserFunc) {
                        e.bossLaserFunc(e);
                    }
                    e.bossTimer -= dt;
                    if (e.bossTimer <= 0.0f) {
                        e.bossLaserActive = false;
                        e.bossState       = BossState::Pause;
                        e.bossTimer       = e.bossPauseDuration;
                        e.bossNextState   = BossState::MoveToPosition;
                        e.bossTargetX     = Randf(CANVAS_W * 0.1f, CANVAS_W * 0.9f);
                    }
                    break;

                default:
                    break;
            }
        }
    }

    if (!bossAlive) {
        g_game.bossActive        = false;
        g_game.progressionPaused = false;
    }
}

// ---------------------------------------------------------------------------
// Playing update (normal + boss)
// ---------------------------------------------------------------------------

static void UpdatePlaying(float dt, const GameInput& in) {
    Player& pl = g_game.player;

    // Movement
    pl.pos.x += in.dx * 200.0f * dt;
    pl.pos.y += in.dy * 200.0f * dt;
    pl.pos.x = std::max(12.0f, std::min(CANVAS_W - 12.0f, pl.pos.x));
    pl.pos.y = std::max(15.0f, std::min(CANVAS_H - 15.0f, pl.pos.y));

    if (pl.invTimer  > 0) pl.invTimer  -= dt;
    if (pl.fireTimer > 0) pl.fireTimer -= dt;

    // Bomb — clears all enemies and their bullets
    if (in.bomb && g_game.bombs > 0) {
        g_game.bombs--;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!g_game.enemies[i].active) continue;
            int t = g_game.enemies[i].type;
            g_game.score += s_ePts[t];
            if (g_game.score > g_game.highScore) g_game.highScore = g_game.score;
            Game_SpawnParticles(g_game.enemies[i].pos, 14, 90.0f, s_eColor[t]);
            g_game.enemies[i].active = false;
        }
        for (int i = 0; i < MAX_BULLETS; i++)
            if (g_game.bullets[i].isEnemy) g_game.bullets[i].active = false;
        Game_SpawnParticles(pl.pos, 24, 160.0f, MakeColor(255, 220, 80));
    }

    // Shooting
    if (in.fire && pl.fireTimer <= 0.0f) {
        int idx = FreeBullet();
        if (idx >= 0) {
            g_game.bullets[idx] = { pl.pos, {0.0f, -380.0f}, false, true };
            pl.fireTimer = 0.12f;
        }
    }

    // Bullets
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet& b = g_game.bullets[i];
        if (!b.active) continue;
        b.pos.x += b.vel.x * dt;
        b.pos.y += b.vel.y * dt;
        if (b.pos.y < -20 || b.pos.y > CANVAS_H + 20 ||
            b.pos.x < -20 || b.pos.x > CANVAS_W + 20)
            b.active = false;
    }

    // If a boss is active, update only boss enemies and collisions,
    // with progression and spawns already paused.
    if (g_game.bossActive) {
        UpdateBossEnemies(dt);
        HandleCollisions();
        return;
    }

    // Normal enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy& e = g_game.enemies[i];
        if (!e.active) continue;

        if (e.type <= 3) {
            e.pos.y += e.vel.y * dt;

            if (e.type == 1) {
                e.phase += 2.0f * dt;
                e.pos.x = e.baseX + sinf(e.phase) * 55.0f;
            } else if (e.type == 3) {
                e.pos.x += e.vel.x * dt;
                if (e.pos.x < 10.0f || e.pos.x > CANVAS_W - 10.0f)
                    e.vel.x = -e.vel.x;
            } else if (e.type == 2) {
                e.shootTimer -= dt;
                if (e.shootTimer <= 0.0f && e.pos.y > 0.0f) {
                    e.shootTimer = Randf(1.5f, 3.0f);
                    int idx = FreeBullet();
                    if (idx >= 0) {
                        float dx  = g_game.player.pos.x - e.pos.x;
                        float dy  = g_game.player.pos.y - e.pos.y;
                        float len = sqrtf(dx * dx + dy * dy);
                        if (len > 0.0f)
                            g_game.bullets[idx] = {
                                e.pos,
                                { dx / len * 160.0f, dy / len * 160.0f },
                                true, true
                            };
                    }
                }
            }

            if (e.pos.y > CANVAS_H + 30.0f) e.active = false;
        }
    }

    // Enemy spawning (only when progression not paused)
    if (!g_game.progressionPaused) {
        g_game.spawnTimer -= dt;
        if (g_game.spawnTimer <= 0.0f) {
            SpawnEnemy();
            g_game.spawnTimer = SpawnInterval();
        }
    }

    HandleCollisions();

    // Boss cycle repeats every 240 seconds
    float cycle = fmodf(g_game.elapsed, 240.0f);

    if (cycle < dt) {
        g_game.boss1Spawned = false;
        g_game.boss2Spawned = false;
    }

    // Boss 1 every cycle at t = 60
    if (!g_game.bossActive &&
        !g_game.progressionPaused &&
        !g_game.boss1Spawned &&   
        cycle >= 60.0f &&
        cycle < 61.0f + dt) {

        // Clear normal enemies
        for (int i = 0; i < MAX_ENEMIES; i++)
            g_game.enemies[i].active = false;

        for (int i = 0; i < MAX_BULLETS; i++)
            if (g_game.bullets[i].isEnemy)
                g_game.bullets[i].active = false;

        SpawnBoss1();
        g_game.bossActive        = true;
        g_game.progressionPaused = true;
        g_game.boss1Spawned = true;
        return;
    }

    // Boss 2 every cycle at t = 180
    if (!g_game.bossActive &&
        !g_game.progressionPaused &&
        !g_game.boss2Spawned &&  
        cycle >= 180.0f &&
        cycle < 181.0f + dt) {

        for (int i = 0; i < MAX_ENEMIES; i++)
            g_game.enemies[i].active = false;

        for (int i = 0; i < MAX_BULLETS; i++)
            if (g_game.bullets[i].isEnemy)
                g_game.bullets[i].active = false;

        SpawnBoss2();
        g_game.bossActive        = true;
        g_game.progressionPaused = true;
        g_game.boss2Spawned = true;
        return;
    }


    // Difficulty / time progression (only when not paused)
    if (!g_game.progressionPaused)
        g_game.elapsed += dt;
}

// ---------------------------------------------------------------------------
// Game_Update — called every frame by render.cpp
// ---------------------------------------------------------------------------

void Game_Update(float dt, const GameInput& input) {
    // Stars and particles run in every state for visual continuity
    float starSpeed = (g_game.state == GameState::Playing) ? 1.0f : 0.25f;
    for (int i = 0; i < NUM_STARS; i++) {
        g_game.stars[i].y += g_game.stars[i].speed * starSpeed * dt;
        if (g_game.stars[i].y > CANVAS_H) {
            g_game.stars[i].y = 0.0f;
            g_game.stars[i].x = Randf(0.0f, CANVAS_W);
        }
    }
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = g_game.particles[i];
        if (!p.active) continue;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.life  -= dt;
        if (p.life <= 0.0f) p.active = false;
    }

    switch (g_game.state) {
        case GameState::Menu:
            if (input.confirm) Game_Reset();
            break;
        case GameState::Playing:
            UpdatePlaying(dt, input);
            break;
        case GameState::GameOver:
            if (input.confirm) Game_Reset();
            break;
        case GameState::EventClose:
            g_game.closeTimer -= dt;
            break;
    }
}
