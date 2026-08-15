#include <cassert>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>
#include "ship.h"
#include "terrain.h"
#include "game.h"
#include "config.h"
#include "moons.h"
#include "geysers.h"
#include "volcanoes.h"
#include "atmosphere.h"
#include "rings.h"
#include "twister.h"
#include "wormhole.h"
#include "tanker.h"
#include "quake.h"
#include "renderer_pc.h"

static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { printf("FAIL: %s (line %d)\n", #cond, __LINE__); return 1; } \
} while (0)

static int testShip()
{
    Ship s;
    s.reset(100, 100);
    CHECK(s.posX == 100 && s.posY == 100);
    CHECK(s.fuel == FUEL_MAX);

    s.setTargetRotation(-45);
    CHECK(s.targetRotation == -45);

    s.setTargetRotation(-100);
    CHECK(s.targetRotation == ROTATION_MIN_DEG);

    s.setTargetRotation(100);
    CHECK(s.targetRotation == ROTATION_MAX_DEG);

    s.setThrust(0.5f);
    CHECK(s.thrustBuild > 0);

    s.setThrust(0);
    for (int i = 0; i < 20; i++) s.update();
    CHECK(s.fuel > FUEL_MAX - 1.0f);

    s.setThrust(1.0f);
    float before = s.fuel;
    s.update();
    CHECK(s.fuel < before);

    s.fuel = 0;
    s.setThrust(1.0f);
    s.update();
    CHECK(s.thrustBuild == 0);

    s.crash();
    CHECK(!s.active);
    CHECK(s.exploding);

    Ship s2;
    s2.reset(50, 50);
    s2.land();
    CHECK(!s2.active);
    CHECK(!s2.exploding);

    return 0;
}

static int testTerrain()
{
    Terrain t;
    t.init();
    const std::vector<TerrainLine> &lines = t.getLines();
    CHECK(lines.size() > 50);
    CHECK(t.getWidth() > 0);

    int landableCount = 0;
    for (int i = 0; i < (int)lines.size(); i++) {
        if (lines[i].landable) landableCount++;
    }
    CHECK(landableCount >= 4);

    int result = t.checkLanding(100, 110, 500, 0, 0.05f, 0.01f);
    CHECK(result == 0);

    t.generate(4);
    const std::vector<TerrainLine> &gl = t.getLines();
    CHECK(gl.size() > 50);
    CHECK(t.getWidth() > 0);

    int gLandable = 0;
    for (int i = 0; i < (int)gl.size(); i++) {
        if (gl[i].landable) gLandable++;
    }
    CHECK(gLandable >= 4);

    return 0;
}

static int testGame()
{
    Game g;
    g.update();
    CHECK(g.state == STATE_WAITING);

    g.input.startPressed = true;
    g.update();
    g.input.startPressed = false;
    CHECK(g.state == STATE_PLAYING);
    CHECK(g.ship.fuel == FUEL_MAX);

    g.input.angle = -PI / 4.0f;
    g.input.thrust = 0;
    for (int i = 0; i < 500; i++) g.update();

    CHECK(g.state == STATE_PLAYING || g.state == STATE_LANDED ||
          g.state == STATE_CRASHED || g.state == STATE_GAMEOVER);

    return 0;
}

static int testLevels()
{
    Game g;
    g.input.startPressed = true;
    g.update();
    g.input.startPressed = false;
    CHECK(g.state == STATE_PLAYING);
    CHECK(g.level == START_LEVEL);
    CHECK(g.introTimer > 0);

    for (int i = 0; i < (int)(LEVEL_INTRO_TIME / GAME_DT) + 1; i++) g.update();
    CHECK(g.introTimer == 0);

    g.ship.fuel = 50.0f;
    g.state = STATE_LANDED;
    g.update();
    CHECK(g.level == START_LEVEL + 1);
    CHECK(g.state == STATE_PLAYING);
    CHECK(g.introTimer > 0);
    CHECK(fabsf(g.ship.fuel - 50.0f) < 1.0f);

    g.state = STATE_LANDED;
    g.ship.fuel = 0.0f;
    g.update();
    CHECK(g.state == STATE_GAMEOVER);

    for (int i = 0; i < (int)(GAMEOVER_RESET_DELAY / GAME_DT) + 1; i++) g.update();
    CHECK(g.state == STATE_WAITING);

    return 0;
}

static int testLandingBonus()
{
    Game g;
    g.input.startPressed = true;
    g.update();
    g.input.startPressed = false;
    for (int i = 0; i < (int)(LEVEL_INTRO_TIME / GAME_DT) + 1; i++) g.update();

    // Classic level-1 terrain: mult-4 pad at lines 34..37.
    auto placeShip = [&](float vy) {
        const std::vector<TerrainLine> &tl = g.terrain.getLines();
        float px = tl[0].x1, padY = tl[0].y1;
        for (size_t i = 0; i + 3 < tl.size(); i++) {
            if (tl[i].landable && tl[i + 1].landable && tl[i + 2].landable &&
                tl[i + 3].landable && tl[i].y1 == tl[i + 3].y1) {
                px = (tl[i].x1 + tl[i + 3].x2) / 2.0f;
                padY = tl[i].y1;
                break;
            }
        }
        g.ship.reset(px, padY - 4.48f);
        g.ship.scale = 0.32f;
        g.ship.velX = 0.0f;
        g.ship.velY = vy;
        g.ship.rotation = 0.0f;
        g.ship.targetRotation = 0.0f;
        g.ship.fuel = 900.0f;
        g.fuel = 900.0f;
        g.input.thrust = 0.0f;
        g.input.angle = 0.0f;
        g.state = STATE_PLAYING;
        g.introTimer = 0.0f;
    };

    // Perfect touchdown: velY well below the threshold. The +50 fuel is NOT
    // granted at the landing spot (the player is focused there) — it shows up
    // as 950 in the FUEL counter at the start of the next level.
    placeShip(0.05f);
    g.update();
    CHECK(g.state == STATE_LANDED);
    CHECK(fabsf(g.ship.fuel - 900.0f) < 1.0f);
    CHECK(g.landPerfectGet());
    for (int i = 0; i < 600 && g.state == STATE_LANDED; i++) g.update();
    CHECK(g.state == STATE_PLAYING);
    CHECK(fabsf(g.ship.fuel - 950.0f) < 1.0f);

    // Hard-but-safe touchdown: velY between the thresholds -> no fuel bonus.
    placeShip(0.10f);
    g.update();
    CHECK(g.state == STATE_LANDED);
    CHECK(fabsf(g.ship.fuel - 900.0f) < 1.0f);
    CHECK(!g.landPerfectGet());
    for (int i = 0; i < 600 && g.state == STATE_LANDED; i++) g.update();
    CHECK(g.state == STATE_PLAYING);
    CHECK(fabsf(g.ship.fuel - 900.0f) < 1.0f);

    return 0;
}

static int testDemo()
{
    srand(1234);
    Game g;
    CHECK(g.state == STATE_WAITING);
    CHECK(!g.demo);
    CHECK(g.demoTimer > 0);

    for (int i = 0; i < (int)(DEMO_START_DELAY / GAME_DT) + 1; i++) g.update();
    CHECK(g.state == STATE_PLAYING);
    CHECK(g.demo);
    CHECK(g.level >= 1 && g.level <= DEMO_MAX_LEVEL);

    bool finished = false;
    bool outcomeSeen = false;
    bool sawWormhole = false;
    for (int i = 0; i < 200000; i++) {
        g.update();
        if (g.wormhole.active() && g.state == STATE_PLAYING) sawWormhole = true;
        if (g.state == STATE_LANDED || g.state == STATE_CRASHED) outcomeSeen = true;
        if (g.state == STATE_WAITING) {
            finished = true;
            break;
        }
    }
    if (sawWormhole) outcomeSeen = true; // showcase: the swallow ended the demo
    CHECK(finished);
    CHECK(outcomeSeen);

    Game g2;
    srand(7);
    for (int i = 0; i < (int)(DEMO_START_DELAY / GAME_DT) + 1; i++) g2.update();
    CHECK(g2.state == STATE_PLAYING && g2.demo);
    g2.input.startPressed = true;
    g2.update();
    CHECK(!g2.demo);
    CHECK(g2.state == STATE_PLAYING);

    return 0;
}

static int testStorm()
{
    Terrain t;
    t.generate(4);
    Storm s;
    s.reset(1);
    CHECK(!s.active());
    int activeCount = 0;
    for (int i = 0; i < 30; i++) {
        s.reset(STORM_START_LEVEL);
        if (s.active()) activeCount++;
    }
    CHECK(activeCount > 0);
    CHECK(activeCount < 30);
    s.reset(STORM_START_LEVEL + 3);
    CHECK(s.activeBolts() == 0);
    CHECK(!s.strikes(0, 0, STORM_HIT_RADIUS));

    s.update(GAME_DT, t);
    return 0;
}

static int testMoon()
{
    CHECK(moonIndex(1) == 0);
    CHECK(moonIndex(8) == 7);
    CHECK(moonIndex(9) == 0);
    CHECK(moonGravity(1) == 1.0f);
    CHECK(moonGravity(3) < 1.0f);

    Ship s;
    s.reset(100, 400);
    s.velY = 0;
    s.gravity = GRAVITY * moonGravity(3);
    float g = s.gravity;
    s.update();
    CHECK(fabsf(s.velY - g) < 1e-6f);
    s.gravity = GRAVITY;
    s.velY = 0;
    s.update();
    CHECK(fabsf(s.velY - GRAVITY) < 1e-6f);

    Game g2;
    g2.input.startPressed = true;
    g2.update();
    g2.input.startPressed = false;
    g2.update();
    CHECK(g2.ship.gravity == GRAVITY * moonGravity(START_LEVEL));
    return 0;
}

static int testGeysers()
{
    CHECK(moonHasGeysers(7));
    CHECK(moonHasGeysers(15));
    CHECK(!moonHasGeysers(1));
    CHECK(!moonHasGeysers(8));

    Geysers g;
    Terrain t;
    t.generate(7);
    g.reset(7, t);
    CHECK(g.active());
    CHECK(g.ventCount() > 0);
    CHECK(!g.inPlume(-50.0f, -50.0f));
    g.reset(1, t);
    CHECK(!g.active());

    g.reset(7, t);
    int maxAlive = 0;
    bool sawPlume = false;
    float vx = g.ventX(0);
    float gy = 500.0f;
    const std::vector<TerrainLine> &tl = t.getLines();
    for (int i = 0; i < (int)tl.size(); i++) {
        if (vx >= tl[i].x1 && vx <= tl[i].x2 && tl[i].x2 != tl[i].x1) {
            float tt = (vx - tl[i].x1) / (tl[i].x2 - tl[i].x1);
            gy = tl[i].y1 + (tl[i].y2 - tl[i].y1) * tt;
            break;
        }
    }
    for (int i = 0; i < 2000; i++) {
        g.update(GAME_DT);
        if (g.particlesAlive() > maxAlive) maxAlive = g.particlesAlive();
        if (g.inPlume(vx, gy - 5.0f)) sawPlume = true;
    }
    CHECK(maxAlive > 0);
    CHECK(sawPlume);
    return 0;
}

static int testVolcanoes()
{
    CHECK(moonHasVolcanoes(2));
    CHECK(moonHasVolcanoes(10));
    CHECK(!moonHasVolcanoes(1));
    CHECK(!moonHasVolcanoes(7));

    Volcanoes v;
    Terrain t;
    t.generate(2);
    v.reset(2, t);
    CHECK(v.active());
    CHECK(v.volcanoCount() > 0);
    v.reset(1, t);
    CHECK(!v.active());

    v.reset(2, t);
    int maxAlive = 0;
    for (int i = 0; i < 3000; i++) {
        v.update(GAME_DT);
        if (v.particlesAlive() > maxAlive) maxAlive = v.particlesAlive();
    }
    CHECK(maxAlive > 0);

    const float vs = SCREEN_H / 700.0f;
    CHECK(v.countInView(0.0f, vs) <= VOLCANO_MAX_VISIBLE);
    CHECK(v.countInView(-200.0f, vs) <= VOLCANO_MAX_VISIBLE);
    for (float vx = -300.0f; vx <= 1200.0f; vx += 37.0f) {
        CHECK(v.countInView(vx, vs) <= VOLCANO_MAX_VISIBLE);
        CHECK(v.countInView(vx, vs * 5.0f) <= VOLCANO_MAX_VISIBLE);
    }
    float zx = v.volcanoX(0);
    CHECK(v.countInView(160.0f - zx * vs * 5.0f, vs * 5.0f) >= 1);
    return 0;
}

static int testVolcanoLava()
{
    // Lava poured onto a landing pad: ranges must stay inside the pad and a
    // free strip of at least VOLCANO_SAFE_STRIP must always remain clear.
    bool sawLava = false;
    for (int s = 0; s < 60; s++) {
        srand(1000 + s);
        Terrain t;
        t.generate(2);
        Volcanoes v;
        v.reset(2, t);
        CHECK(v.active());

        const std::vector<TerrainLine> &tl = t.getLines();
        std::vector<std::pair<float, float> > pads;
        for (int k = 0; k < (int)tl.size(); k++) {
            if (!tl[k].landable || tl[k].multiplier <= 1) continue;
            float a = tl[k].x1, b = tl[k].x2;
            int j = k;
            while (j + 1 < (int)tl.size() && tl[j + 1].landable && tl[j + 1].multiplier > 1) {
                j++;
                b = tl[j].x2;
            }
            pads.push_back(std::make_pair(a, b));
            k = j;
        }

        for (int i = 0; i < (int)pads.size(); i++) {
            float pw = pads[i].second - pads[i].first;
            float covered = 0.0f;
            for (int q = 0; q < v.lavaRangeCount(); q++) {
                float x1 = v.lavaRangeX1(q), x2 = v.lavaRangeX2(q);
                float lo = x1 > pads[i].first ? x1 : pads[i].first;
                float hi = x2 < pads[i].second ? x2 : pads[i].second;
                if (hi > lo) {
                    covered += hi - lo;
                    CHECK(x1 >= pads[i].first - 0.5f);
                    CHECK(x2 <= pads[i].second + 0.5f);
                }
            }
            if (covered <= 0.0f) continue;
            sawLava = true;
            CHECK(pw - covered >= VOLCANO_SAFE_STRIP - 0.5f);

            bool onLava = false;
            for (int q = 0; q < v.lavaRangeCount(); q++) {
                float lo = v.lavaRangeX1(q) > pads[i].first ? v.lavaRangeX1(q) : pads[i].first;
                float hi = v.lavaRangeX2(q) < pads[i].second ? v.lavaRangeX2(q) : pads[i].second;
                if (hi > lo) {
                    float cx = (lo + hi) * 0.5f;
                    CHECK(v.landOnLava(cx - 1.0f, cx + 1.0f));
                    onLava = true;
                }
            }
            CHECK(onLava);
            CHECK(!v.landOnLava(pads[i].second + 30.0f, pads[i].second + 32.0f));
        }
    }
    CHECK(sawLava);

    // Game integration: a perfect touchdown on the covered part of a pad burns
    // the ship (STATE_CRASHED); the same landing on the clear strip succeeds.
    bool sawBurn = false, sawSafe = false;
    for (int s = 0; s < 60 && !(sawBurn && sawSafe); s++) {
        srand(9000 + s);
        Game g;
        g.input.startPressed = true;
        g.update();
        g.input.startPressed = false;
        CHECK(g.state == STATE_PLAYING);
        CHECK(g.level == START_LEVEL);
        g.introTimer = 0;
        g.demo = false;
        // Force an Io level regardless of START_LEVEL so lava is guaranteed.
        g.level = 2;
        g.terrain.generate(g.level);
        g.volcanoes.reset(g.level, g.terrain);

        const std::vector<TerrainLine> &tl = g.terrain.getLines();
        std::vector<std::pair<float, float> > pads;
        for (int k = 0; k < (int)tl.size(); k++) {
            if (!tl[k].landable || tl[k].multiplier <= 1) continue;
            float a = tl[k].x1, b = tl[k].x2;
            int j = k;
            while (j + 1 < (int)tl.size() && tl[j + 1].landable && tl[j + 1].multiplier > 1) {
                j++;
                b = tl[j].x2;
            }
            pads.push_back(std::make_pair(a, b));
            k = j;
        }

        for (int p = 0; p < (int)pads.size(); p++) {
            float padX1 = pads[p].first, padX2 = pads[p].second;
            std::vector<float> covLo, covHi;
            for (int q = 0; q < g.volcanoes.lavaRangeCount(); q++) {
                float lo = g.volcanoes.lavaRangeX1(q) > padX1 ? g.volcanoes.lavaRangeX1(q) : padX1;
                float hi = g.volcanoes.lavaRangeX2(q) < padX2 ? g.volcanoes.lavaRangeX2(q) : padX2;
                if (hi > lo) { covLo.push_back(lo); covHi.push_back(hi); }
            }
            if (covLo.empty()) continue;

            float burnX = (covLo[0] + covHi[0]) * 0.5f;
            float safeX = -1.0f;
            for (float x = padX1 + 0.5f; x <= padX2 - 0.5f; x += 0.5f) {
                bool covered = false;
                for (int q = 0; q < (int)covLo.size(); q++) {
                    if (x + 3.2f > covLo[q] && x - 3.2f < covHi[q]) { covered = true; break; }
                }
                if (!covered) { safeX = x; break; }
            }
            if (safeX < 0.0f) continue;
            float padY = -1.0f;
            for (int k = 0; k < (int)tl.size(); k++) {
                if (burnX >= tl[k].x1 && burnX <= tl[k].x2) { padY = tl[k].y1; break; }
            }
            if (padY < 0.0f) continue;

            auto placeShip = [&](float px) {
                g.ship.reset(px, padY - 4.48f);
                g.ship.scale = 0.32f;
                g.ship.velX = 0.0f;
                g.ship.velY = 0.0f;
                g.ship.rotation = 0.0f;
                g.ship.targetRotation = 0.0f;
                g.ship.fuel = 900.0f;
                g.input.thrust = 0.0f;
                g.input.angle = 0.0f;
                g.state = STATE_PLAYING;
                g.introTimer = 0.0f;
            };

            placeShip(burnX);
            g.update();
            if (g.state == STATE_CRASHED) sawBurn = true;

            placeShip(safeX);
            g.update();
            if (g.state == STATE_LANDED) sawSafe = true;
        }
    }
    CHECK(sawBurn);
    CHECK(sawSafe);
    return 0;
}

static int testAtmosphere()
{
    srand(42);
    CHECK(moonHasTitan(6));
    CHECK(moonHasTitan(14));
    CHECK(moonHasTitan(30));
    CHECK(!moonHasTitan(1));
    CHECK(!moonHasTitan(5));

    Atmosphere a;
    a.reset(6);
    CHECK(a.active());
    CHECK(a.bandCount() == FOG_BAND_COUNT);
    // At the band centers the ship is always hidden (drift < min half).
    CHECK(a.hidesShip(200.0f, a.bandCenter(0)));
    CHECK(a.hidesShip(500.0f, a.bandCenter(1)));
    // Far from every band it is never hidden (well below the lowest band and
    // well above the highest).
    CHECK(!a.hidesShip(400.0f, 900.0f));
    CHECK(!a.hidesShip(400.0f, 50.0f));
    // The blind zones move: sampling a grid, both hidden and clear points
    // exist, and the pattern is not the same after time has passed.
    bool sawClear = false;
    for (float y = 50.0f; y < 700.0f; y += 4.0f) {
        if (!a.hidesShip(400.0f, y)) sawClear = true;
    }
    CHECK(sawClear);
    int hidT0 = 0;
    for (float x = 0.0f; x < 900.0f; x += 30.0f) {
        for (float y = 180.0f; y < 500.0f; y += 30.0f) {
            if (a.hidesShip(x, y)) hidT0++;
        }
    }
    for (int i = 0; i < 2000; i++) a.update(GAME_DT);
    int hidT1 = 0;
    for (float x = 0.0f; x < 900.0f; x += 30.0f) {
        for (float y = 180.0f; y < 500.0f; y += 30.0f) {
            if (a.hidesShip(x, y)) hidT1++;
        }
    }
    CHECK(hidT0 > 0);
    CHECK(hidT0 != hidT1);
    a.reset(1);
    CHECK(!a.active());
    CHECK(!a.hidesShip(200.0f, 250.0f));

    a.reset(6);
    for (int i = 0; i < 3000; i++) a.update(GAME_DT);
    CHECK(a.active());

    // The fog and the terrain halo must never be drawn over the HUD strip
    // (top of the screen): with a zoomed approach camera that pushes a band
    // across the top, the whole strip above FOG_SCREEN_TOP stays black.
    {
        Terrain t;
        t.generate(6);
        float vs = SCREEN_H / 700.0f * 5.0f;
        float vx = 400.0f;
        float vy = 100.0f - (FOG_BAND_START + FOG_BAND_HALF) * vs;
        RendererPC fr((int)SCREEN_W, (int)SCREEN_H, "");
        fr.clear();
        a.reset(6);
        a.drawSky(fr, t, vx, vy, vs);
        const uint8_t *fb = fr.data();
        for (int y = 0; y < FOG_SCREEN_TOP; y++) {
            for (int x = 0; x < 320; x++) {
                if (fb[y * 320 + x] != 0) {
                    printf("FAIL fog/halo above HUD at (%d,%d) luma %d\n",
                           x, y, (int)fb[y * 320 + x]);
                    return 1;
                }
            }
        }
        // sanity: the band really overlapped the top strip (clamp was active)
        bool clampActive = false;
        for (int x = 0; x < 320; x += 2) {
            if (fb[FOG_SCREEN_TOP * 320 + x] != 0) {
                clampActive = true;
                break;
            }
        }
        CHECK(clampActive);
    }

    Game g;
    g.newGame();
    CHECK(!g.atmosphere.active());
    for (int i = 0; i < 8 && !moonHasTitan(g.level); i++) g.nextLevel();
    CHECK(moonHasTitan(g.level));
    CHECK(g.atmosphere.active());
    CHECK(!g.storm.active());
    g.atmosphere.update(GAME_DT);
    return 0;
}

static int testRings()
{
    CHECK(moonHasRings(4));    // GANYMEDES (moonIndex 3 -> level 4)
    CHECK(moonHasRings(12));
    CHECK(moonHasRings(20));
    CHECK(!moonHasRings(1));   // LUNA
    CHECK(!moonHasRings(3));   // EUROPA
    CHECK(!moonHasRings(6));   // TITAN

    Rings r;
    Terrain t;
    t.generate(4);
    r.reset(4, t);
    CHECK(r.active());
    CHECK(r.ringCount() == RING_COUNT);
    CHECK(r.rocksInRing() == RING_SMALL_COUNT + RING_DANGER_COUNT);
    CHECK(r.rockDanger(0) == false);
    CHECK(r.rockDanger(RING_SMALL_COUNT) == true);
    CHECK(r.rockDanger(RING_SMALL_COUNT + RING_DANGER_COUNT) == false);
    r.reset(1, t);
    CHECK(!r.active());

    r.reset(4, t);
    // Some rock must be visible above the terrain and another reachable to hit.
    float wx = 0, wy = 0;
    bool sawVisible = false;
    for (int i = 0; i < r.rocksInRing() && !sawVisible; i++) {
        if (r.rockVisible(t, i, wx, wy)) { sawVisible = true; }
    }
    CHECK(sawVisible);
    // Park the ship exactly on the first DANGER rock -> guaranteed hit.
    float rx = 0, ry = 0;
    for (int i = 0; i < r.rocksInRing(); i++) {
        if (r.rockVisible(t, i, rx, ry) && r.rockDanger(i)) break;
    }
    CHECK(r.hitsShip(t, rx, ry, RING_SHIP_RADIUS));
    // Far above the highest possible band (terrain top - RING_HEIGHT_HIGH) ->
    // never hit.
    CHECK(!r.hitsShip(t, 400.0f, 10.0f, RING_SHIP_RADIUS));

    // Rings move over time (positions are a function of the advancing phase).
    float p0x = 0, p0y = 0, p1x = 999, p1y = 999;
    bool vis0 = false, vis1 = false;
    for (int i = 0; i < r.rocksInRing() && !vis0; i++)
        if (r.rockVisible(t, i, p0x, p0y)) { vis0 = true; }
    CHECK(vis0);
    for (int i = 0; i < 2000; i++) r.update(GAME_DT);
    for (int i = 0; i < r.rocksInRing() && !vis1; i++)
        if (r.rockVisible(t, i, p1x, p1y)) { vis1 = true; }
    CHECK(vis1);
    CHECK(p0x != p1x || p0y != p1y);

    // Game integration: at a Ganymede level the rings are active; a nearby
    // non-Ganymede level (e.g. LUNA level 1) keeps them off.
    Game g;
    g.newGame();
    bool started = moonHasRings(g.level);
    CHECK(g.rings.active() == started);
    int guard = 0;
    while (moonHasRings(g.level) && guard < 8) { g.nextLevel(); guard++; }
    if (moonHasRings(g.level)) { // newGame may have started on Ganymede; step on
        for (int i = 0; i < 8 && moonHasRings(g.level); i++) g.nextLevel();
    }
    CHECK(!moonHasRings(g.level));
    CHECK(!g.rings.active());
    for (int i = 0; i < 8 && !moonHasRings(g.level); i++) g.nextLevel();
    CHECK(moonHasRings(g.level));
    CHECK(g.rings.active());
    g.rings.update(GAME_DT);
    return 0;
}

static int testTwister()
{
    // Only Triton levels (moonIndex 7 -> level 8, 16, 24...) activate it.
    CHECK(moonHasTwister(8));
    CHECK(moonHasTwister(16));
    CHECK(moonHasTwister(24));
    CHECK(!moonHasTwister(1));   // LUNA
    CHECK(!moonHasTwister(2));   // IO
    CHECK(!moonHasTwister(7));   // ENCELADUS

    Twister tw;
    Terrain t;
    t.generate(8);
    tw.reset(8, t);
    CHECK(tw.active());
    CHECK(tw.coreX() > 40.0f);
    CHECK(tw.strength() >= TWISTER_STRENGTH_MIN);
    CHECK(tw.strength() <= TWISTER_STRENGTH_MAX);

    // Level with no twister stays off.
    tw.reset(1, t);
    CHECK(!tw.active());

    // Physics: a ship parked inside the radius is dragged toward the core and
    // downward (velocity gains a strong inward/down component) and captured.
    tw.reset(8, t);
    Ship s;
    s.reset(tw.coreX() + 60.0f, tw.coreY(t) - 80.0f);
    s.velX = 0.0f;
    s.velY = 0.0f;
    float y0 = s.posY;
    float maxRot = 0.0f;
    for (int i = 0; i < 60; i++) {
        s.update();       // integrate position + gravity (as in Game::update)
        tw.update(GAME_DT);
        tw.apply(s, t);
        if (fabsf(s.rotation) > maxRot) maxRot = fabsf(s.rotation);
    }
    CHECK(tw.captured());
    CHECK(s.posY > y0);        // it sank toward the ground
    CHECK(maxRot > 5.0f);      // the nose rocked while captured
    CHECK(maxRot < 85.0f);     // never reached full +/-90 authority
    CHECK(sqrtf((s.posX - tw.coreX()) * (s.posX - tw.coreX()) +
                (s.posY - tw.coreY(t)) * (s.posY - tw.coreY(t))) < TWISTER_RADIUS);

    // A ship high in the funnel with sustained outward radial thrust breaks
    // free (not sucked back): it must keep outward radial motion above the
    // escape velocity for TWISTER_ESCAPE_TICKS ticks, then it is flung out.
    // Shallow enough that full power can still fight the depth-scaled grip.
    tw.reset(8, t);
    Ship s2;
    s2.reset(tw.coreX() - 80.0f, tw.coreY(t) - 120.0f);
    s2.velX = -0.25f; // already leaving
    s2.velY = 0.0f;
    s2.thrustBuild = 1.0f;
    float dx = s2.posX - tw.coreX(), dy = s2.posY - tw.coreY(t);
    s2.rotation = atan2f(dx, -dy) * 180.0f / PI; // head straight outward
    bool escaped = false;
    for (int i = 0; i < TWISTER_ESCAPE_TICKS + 15 && !escaped; i++) {
        tw.apply(s2, t);
        if (tw.justEscaped()) escaped = true;
    }
    CHECK(escaped);
    CHECK(!tw.captured());
    float edx = s2.posX - tw.coreX(), edy = s2.posY - tw.coreY(t);
    float ed = sqrtf(edx * edx + edy * edy);
    float outVel = (s2.velX * edx + s2.velY * edy) / ed;
    CHECK(outVel > 0.1f); // flung outward along the exit direction

    // Game integration: reaching a Triton level activates the twister.
    Game g;
    g.newGame();
    for (int i = 0; i < 8 && !moonHasTwister(g.level); i++) g.nextLevel();
    CHECK(moonHasTwister(g.level));
    CHECK(g.twister.active());
    g.twister.update(GAME_DT);
    return 0;
}

static int testTanker()
{
    srand(42);
    Terrain t1;
    t1.generate(1);
    Tanker tn1;
    tn1.reset(1, t1, 100.0f);
    if (!TANKER_FORCE_LEVEL1) CHECK(!tn1.active);

    // A full tank never summons the tanker.
    Tanker tnFull;
    tnFull.reset(2, t1, FUEL_MAX);
    CHECK(!tnFull.active);

    // Forced spawn bypasses level/chance gates, but NEVER the fuel rule: with
    // a full tank no tanker shows up. A low tank can be force-spawned.
    bool fullForced = false;
    for (int s = 0; s < 50 && !fullForced; s++) {
        srand(20000 + s);
        Terrain tfF;
        tfF.generate(2);
        Tanker tkFull;
        tkFull.reset(2, tfF, FUEL_MAX, true);
        if (tkFull.active) fullForced = true;
    }
    CHECK(!fullForced);

    bool forcedSpawn = false;
    for (int s = 0; s < 200 && !forcedSpawn; s++) {
        srand(10000 + s);
        Terrain tf;
        tf.generate(2);
        Tanker tkf;
        tkf.reset(2, tf, FUEL_MAX * 0.4f, true);
        if (tkf.active) forcedSpawn = true;
    }
    CHECK(forcedSpawn);

    // Ganymede debris rings: no tanker there, even forced.
    srand(7);
    Terrain tg;
    tg.generate(4);
    Tanker tnG;
    tnG.reset(4, tg, 100.0f, true);
    CHECK(!tnG.active);

    // Titan: the tanker hovers between the two fog bands, closer to the first.
    bool titanSeen = false;
    for (int s = 0; s < 200 && !titanSeen; s++) {
        srand(5000 + s);
        Terrain tt;
        tt.generate(6);
        Tanker tkT;
        tkT.reset(6, tt, 100.0f, true);
        if (!tkT.active) continue;
        titanSeen = true;
        CHECK(tkT.baseY == TANKER_TITAN_Y);
        CHECK(fabsf(tkT.bodyY - tkT.baseY) <= 0.01f);
        CHECK(tkT.portY > tkT.bodyY);
    }
    CHECK(titanSeen);

    srand(42);
    bool spawned = false;
    for (int seed = 0; seed < 200 && !spawned; seed++) {
        srand(seed);
        Terrain t;
        t.generate(2);
        Tanker tk;
        tk.reset(2, t, 100.0f);
        if (!tk.active) continue;

        spawned = true;

        CHECK(tk.portY > tk.bodyY);
        CHECK(tk.baseY > 0.0f);
        CHECK(fabsf(tk.bodyY - tk.baseY) <= TANKER_BOB_AMP + 0.01f);
        CHECK(tk.bodyY <= 500.0f);

        float terrainW = t.getWidth();
        CHECK(tk.bodyX >= 0.0f);
        CHECK(tk.bodyX <= terrainW);

        Ship dummy;
        dummy.reset(20.0f, 20.0f);

        float x0 = tk.bodyX;
        for (int i = 0; i < 60; i++) tk.update(GAME_DT, dummy);
        CHECK(fabsf(tk.bodyX - x0) > 0.01f);
        CHECK(fabsf(tk.bodyX - x0) <= TANKER_DRIFT_SPEED * GAME_DT * 60.0f + 1.0f);

        // Belly dock: rotation 0 lines the module probe up with the underside
        // drogue (the ship center sits NOZZLE_LEN above the drogue).
        Ship ship;
        ship.reset(tk.drogueX(), tk.drogueY() + TANKER_NOZZLE_LEN);
        ship.fuel = 100.0f;
        ship.scale = 1.0f;
        ship.velY = 0.02f;
        ship.velX = 0.02f;

        // The drogue hangs clear of the hull at the end of its hose.
        CHECK(fabsf(tk.drogueX() - tk.bodyX) <= TANKER_DROGUE_SWAY + 0.01f);
        CHECK(tk.drogueY() > tk.portY);

        CHECK(tk.checkDock(ship));
        CHECK(tk.active);

        tk.beginDock(ship.velX, ship.velY);
        CHECK(tk.docked);
        CHECK(!tk.fuelFlowing);

        float beforeFuel = ship.fuel;
        CHECK(beforeFuel < FUEL_MAX);

        // For the first second fuel does NOT flow yet: it is a hold mini-game.
        for (int i = 0; i < (int)(TANKER_DOCK_LOCK_TIME / GAME_DT) - 5; i++) {
            tk.update(GAME_DT, ship);
        }
        CHECK(ship.fuel == beforeFuel);
        CHECK(!tk.fuelFlowing);
        for (int i = 0; i < 10; i++) tk.update(GAME_DT, ship);
        CHECK(tk.fuelFlowing);

        // Fuel flows incrementally after the lock: a short connection only tops
        // up partway.
        float afterLockFuel = ship.fuel;
        for (int i = 0; i < 60; i++) {
            tk.update(GAME_DT, ship);
        }
        CHECK(ship.fuel > afterLockFuel);
        CHECK(ship.fuel < FUEL_MAX);
        CHECK(tk.docked);

        // Breakaway: firing the engine releases the probe early, keeps the
        // partial fuel and leaves the tanker on station for a reconnect.
        tk.breakAway(ship);
        CHECK(!tk.docked);
        CHECK(!tk.fuelFlowing);
        CHECK(ship.velY > 0.0f);
        CHECK(!tk.leaving);
        CHECK(!tk.done);

        // Reconnect and stay plugged in until the tank is full.
        ship.reset(tk.drogueX(), tk.drogueY() + TANKER_NOZZLE_LEN);
        ship.fuel = beforeFuel;
        ship.scale = 1.0f;
        ship.velY = 0.02f;
        ship.velX = 0.02f;
        CHECK(tk.checkDock(ship));
        tk.beginDock(ship.velX, ship.velY);
        CHECK(tk.docked);
        for (int i = 0; i < 700; i++) {
            tk.update(GAME_DT, ship);
        }

        CHECK(ship.fuel == FUEL_MAX);
        CHECK(ship.fuel > beforeFuel);
        CHECK(ship.velY == 0.0f);
        CHECK(ship.velX == 0.0f);
        CHECK(tk.leaving);
        CHECK(!tk.docked);

        // Breakaway on a bad alignment: push the probe outside the break
        // tolerance with the joystick nudge and confirm the link auto-releases
        // after BREAK_TIME.
        Tanker tkBr;
        tkBr.reset(2, t, 100.0f, true);
        Ship shipBr;
        shipBr.reset(tkBr.drogueX(), tkBr.drogueY() + TANKER_NOZZLE_LEN);
        shipBr.scale = 1.0f;
        shipBr.velX = 20.0f; // held strong sideways nudge
        shipBr.velY = 0.0f;
        tkBr.beginDock(shipBr.velX, shipBr.velY);
        CHECK(tkBr.docked);
        for (int i = 0; i < (int)(TANKER_DOCK_BREAK_TIME / GAME_DT) + 5; i++) {
            tkBr.update(GAME_DT, shipBr);
            shipBr.velX = 20.0f; // player keeps the stick deflected
            shipBr.velY = 0.0f;
        }
        CHECK(!tkBr.docked);

        float startX = tk.bodyX;
        for (int i = 0; i < 9000 && !tk.done; i++) {
            tk.update(GAME_DT, ship);
        }
        CHECK(tk.done);
        CHECK(tk.bodyX > startX);

        Tanker tk2;
        tk2.reset(2, t, 100.0f, true);
        CHECK(tk2.active == true);
    }
    CHECK(spawned);

    srand(99);
    Terrain t2;
    t2.generate(3);
    Tanker tk3;
    tk3.reset(3, t2, 100.0f);
    if (tk3.active) {
        Ship ship3;
        ship3.reset(tk3.drogueX() + 200.0f, tk3.drogueY() + TANKER_NOZZLE_LEN);
        ship3.scale = 1.0f;
        ship3.velY = 0.02f;
        CHECK(!tk3.checkDock(ship3));

        Ship ship3b;
        ship3b.reset(tk3.drogueX(), tk3.drogueY() + TANKER_NOZZLE_LEN);
        ship3b.scale = 1.0f;
        ship3b.velY = 0.5f;
        CHECK(!tk3.checkDock(ship3b));

        Ship ship3c;
        ship3c.reset(tk3.drogueX(), tk3.drogueY() + TANKER_NOZZLE_LEN);
        ship3c.scale = 1.0f;
        ship3c.velY = 0.02f;
        ship3c.velX = 0.5f;
        CHECK(!tk3.checkDock(ship3c));

        // Wrong rotation: the probe points sideways instead of up into the
        // drogue basket. The module is placed left of the drogue with rotation
        // 90, so the horizontal probe tip lands far out of the (now forgiving)
        // horizontal tolerance.
        Ship shipW;
        shipW.reset(tk3.drogueX() - 30.0f, tk3.drogueY() + TANKER_NOZZLE_LEN);
        shipW.rotation = 90.0f;
        shipW.targetRotation = 90.0f;
        shipW.scale = 1.0f;
        shipW.velY = 0.02f;
        shipW.velX = 0.02f;
        CHECK(!tk3.checkDock(shipW));
    }

    // Touching the mothership hull is lethal for both ships; a destroyed
    // tanker no longer collides.
    srand(123);
    for (int s = 0; s < 300; s++) {
        Terrain td;
        td.generate(2);
        Tanker tkd;
        tkd.reset(2, td, 100.0f, true);
        if (!tkd.active) continue;
        CHECK(tkd.hitsHull(tkd.bodyX, tkd.bodyY));
        CHECK(tkd.hitsHull(tkd.bodyX, tkd.bodyY - 60.0f) == false);
        CHECK(tkd.hitsHull(tkd.bodyX + 200.0f, tkd.bodyY) == false);
        tkd.destroy();
        CHECK(!tkd.active);
        CHECK(tkd.done);
        CHECK(!tkd.hitsHull(tkd.bodyX, tkd.bodyY));
        break;
    }
    return 0;
}

static int testParachute()
{
    // Braking to the sink: with the canopy fully open the fall converges on
    // PARACHUTE_SINK instead of accelerating toward TOP_SPEED.
    Ship s;
    s.reset(400, 60);
    s.chute = true;
    s.velY = 0.3f;
    for (int i = 0; i < 300; i++) s.update();
    CHECK(fabsf(s.chuteOpen - 1.0f) < 1e-3f);
    CHECK(s.velY <= PARACHUTE_SINK + 1e-4f);

    // Engine flare while deployed pushes below the sink (perfect landing).
    Ship f;
    f.reset(400, 60);
    f.chute = true;
    f.chuteOpen = 1.0f;
    f.velY = 0.3f;
    f.setThrust(1.0f);
    for (int i = 0; i < 800; i++) f.update();
    CHECK(f.velY < PARACHUTE_SINK);

    // Horizontal drift is capped by the canopy.
    Ship d;
    d.reset(400, 60);
    d.chute = true;
    d.chuteOpen = 1.0f;
    d.velX = 2.0f;
    d.update();
    CHECK(fabsf(d.velX) <= PARACHUTE_DRIFT_MAX + 1e-5f);

    // Wind works as a sail: twice the push while the chute is open.
    Ship w1, w2;
    w1.reset(400, 60); w2.reset(400, 60);
    w1.velX = w2.velX = 0.0f;
    w1.windStrength = w2.windStrength = 0.5f;
    w1.windDir = w2.windDir = 1;
    w1.chute = false; w2.chute = true;
    w2.chuteOpen = 1.0f;
    w1.update(); w2.update();
    CHECK(fabsf(w2.velX - 2.0f * w1.velX) < 1e-4f);

    // reset() clears the chute so each level starts with it available again.
    Ship r;
    r.reset(400, 60);
    r.chute = true; r.chuteOpen = 1.0f;
    r.reset(400, 60);
    CHECK(!r.chute && r.chuteOpen == 0.0f);

    // Deploy through the game: high in the descent the Start button deploys.
    Game g;
    g.newGame();
    for (int i = 0; i < 300; i++) g.update();
    CHECK(g.state == STATE_PLAYING);
    CHECK(g.ship.altitude > PARACHUTE_MIN_ALT);
    g.input.startPressed = true;
    g.update();
    CHECK(g.ship.chute);

    // One-shot: a second press while already deployed does nothing.
    g.input.startPressed = true;
    g.update();
    CHECK(g.ship.chute);

    // Deploy refused too low: below PARACHUTE_MIN_ALT the button flashes a
    // warning without opening the canopy.
    Game g2;
    g2.newGame();
    g2.tanker.done = true;
    for (int i = 0; i < 300; i++) g2.update();
    CHECK(g2.state == STATE_PLAYING);
    g2.ship.altitude = PARACHUTE_MIN_ALT - 20.0f;
    g2.input.startPressed = true;
    g2.update();
    CHECK(!g2.ship.chute);
    CHECK(g2.chuteTooLow() > 0.0f);

    return 0;
}

static int testWormhole()
{
    // The wormhole only hosts on effect-free moons (never combines with
    // another effect): LUNA/CALLISTO yes, the rest no.
    CHECK(moonEffectFree(1) == true);   // LUNA
    CHECK(moonEffectFree(2) == false);  // IO (volcanoes)
    CHECK(moonEffectFree(3) == false);  // EUROPA (acid rain)
    CHECK(moonEffectFree(4) == false);  // GANYMEDES (rings)
    CHECK(moonEffectFree(5) == true);   // CALLISTO
    CHECK(moonEffectFree(6) == false);  // TITAN (fog)
    CHECK(moonEffectFree(7) == false);  // ENCELADUS (geysers)
    CHECK(moonEffectFree(8) == false);  // TRITON (twister)
    CHECK(moonEffectFree(9) == true);   // LUNA (back to the cycle)

    // Fade-in (EMERGING) then the field goes ACTIVE and STAYS active (no
    // fade-out timer): it only dies after a swallow.
    Wormhole w;
    CHECK(!w.active());
    w.reset(400.0f, 200.0f);
    CHECK(w.active());
    CHECK(w.phase() == WH_EMERGING);
    CHECK(w.coreX() == 400.0f && w.coreY() == 200.0f);
    for (int i = 0; i < 130; i++) w.update(GAME_DT); // EMERGING is 1.2 s
    CHECK(w.phase() == WH_ACTIVE);
    for (int i = 0; i < 600; i++) w.update(GAME_DT); // persistent, never fades
    CHECK(w.phase() == WH_ACTIVE);
    CHECK(w.active());
    CHECK(!w.swallowed());

    // Pure radial pull in the OUTER zone (d >= CAPTURE_R): the acceleration is
    // directed toward the core and GROWS the closer to it
    // (a = PULL_MAX*(1-d/GRAB_R)).
    float aFar = 0.0f, aNear = 0.0f;
    {
        Wormhole wF;
        wF.reset(400.0f, 200.0f);
        for (int i = 0; i < 130; i++) wF.update(GAME_DT);
        Ship sF;
        sF.reset(550.0f, 200.0f); // d=150, outer zone (pull only)
        sF.velX = sF.velY = 0.0f;
        float vx0 = sF.velX, vy0 = sF.velY;
        wF.apply(sF);
        float dx = sF.velX - vx0, dy = sF.velY - vy0;
        aFar = sqrtf(dx * dx + dy * dy);
        CHECK(aFar > 0.0f);
        CHECK(!wF.captured());
        CHECK(!wF.swallowed());
        // direction: toward the core (dx<0 since the core is to the left)
        CHECK(dx < 0.0f);
    }
    {
        Wormhole wN;
        wN.reset(400.0f, 200.0f);
        for (int i = 0; i < 130; i++) wN.update(GAME_DT);
        Ship sN;
        sN.reset(510.0f, 200.0f); // d=110, still outer zone
        sN.velX = sN.velY = 0.0f;
        float vx0 = sN.velX, vy0 = sN.velY;
        wN.apply(sN);
        float dx = sN.velX - vx0, dy = sN.velY - vy0;
        aNear = sqrtf(dx * dx + dy * dy);
        CHECK(!wN.captured());
    }
    CHECK(aNear > aFar); // closer -> stronger pull

    // Outside the action radius there is no force at all.
    {
        Wormhole wO;
        wO.reset(400.0f, 200.0f);
        for (int i = 0; i < 130; i++) wO.update(GAME_DT);
        Ship sO;
        sO.reset(650.0f, 200.0f); // d=250 > GRAB_R
        sO.velX = sO.velY = 0.0f;
        wO.apply(sO);
        CHECK(sO.velX == 0.0f && sO.velY == 0.0f);
        CHECK(!wO.swallowed());
    }

    // Manual integration helper: pull + (optional) full thrust away from the
    // core each tick, no gravity (isolates the field). Returns the final
    // distance to the core.
    auto sim = [](Wormhole &wh, Ship &sh, int ticks, bool thrustAway) {
        for (int i = 0; i < ticks && !wh.swallowed(); i++) {
            wh.update(GAME_DT);
            if (thrustAway && !wh.swallowed()) {
                float dx = sh.posX - wh.coreX(), dy = sh.posY - wh.coreY();
                float d = sqrtf(dx * dx + dy * dy);
                if (d > 1.0f) {
                    sh.velX += dx / d * THRUST_ACCEL; // push OUTWARD, away
                    sh.velY += dy / d * THRUST_ACCEL; // from the center
                }
            }
            if (wh.active() && !wh.swallowed()) wh.apply(sh);
            sh.posX += sh.velX;
            sh.posY += sh.velY;
            sh.velX *= DRAG;
            sh.velY *= DRAG;
        }
        float dx = sh.posX - wh.coreX(), dy = sh.posY - wh.coreY();
        return sqrtf(dx * dx + dy * dy);
    };

    // ESCAPE: from the outer reach, thrusting away from the center breaks free.
    {
        Wormhole wE;
        wE.reset(400.0f, 200.0f);
        for (int i = 0; i < 130; i++) wE.update(GAME_DT);
        Ship sE;
        sE.reset(540.0f, 200.0f); // d=140, well outside the no-return zone
        sE.velX = sE.velY = 0.0f;
        float dEnd = sim(wE, sE, 2000, true);
        CHECK(!wE.swallowed());
        CHECK(dEnd > WORMHOLE_GRAB_R); // escaped the action radius
    }

    // NO ESCAPE: inside the capture radius the ship is trapped even with full
    // thrust away -- the vortex scripts it and it is swallowed at the core.
    {
        Wormhole wN;
        wN.reset(400.0f, 200.0f);
        for (int i = 0; i < 130; i++) wN.update(GAME_DT);
        Ship sN;
        sN.reset(440.0f, 220.0f); // d~=44.7 < CAPTURE_R (100)
        sN.velX = sN.velY = 0.0f;
        sim(wN, sN, 1500, true);
        CHECK(wN.captured());
        CHECK(wN.swallowed());
    }

    // VORTEX MECHANICS: crossing CAPTURE_R captures the ship; the spiral keeps
    // shrinking its radius toward the core rim, the ship shrinks, and even
    // continuous full thrust outward cannot stop the swallow.
    {
        Wormhole wV;
        wV.reset(400.0f, 200.0f);
        for (int i = 0; i < 130; i++) wV.update(GAME_DT);
        Ship sV;
        sV.reset(480.0f, 200.0f); // d=80 < CAPTURE_R
        sV.velX = sV.velY = 0.0f;
        sV.scale = 1.5f;
        bool sawShrink = false;
        bool sawCloser = false;
        float prevD = 80.0f;
        for (int i = 0; i < 1500 && !wV.swallowed(); i++) {
            wV.update(GAME_DT);
            if (wV.active() && !wV.swallowed()) {
                wV.apply(sV);
                if (wV.captured() && sV.scale < 1.5f) sawShrink = true;
            }
            if (wV.captured() && !wV.swallowed()) {
                float d = sqrtf((sV.posX - wV.coreX()) * (sV.posX - wV.coreX()) +
                                (sV.posY - wV.coreY()) * (sV.posY - wV.coreY()));
                if (d < prevD) sawCloser = true;
                prevD = d;
            }
        }
        CHECK(wV.captured());
        CHECK(wV.swallowed());
        CHECK(sawShrink);   // the ship shrank during the vortex
        CHECK(sawCloser);   // the spiral closed in toward the core
    }

    // VORTEX CONTINUITY: the first scripted position must stay at the ship's
    // actual capture spot, NOT teleport to the mirrored point of the ellipse.
    // Horizontal case: the old bug placed a ship captured on the left at the
    // symmetric x on the right (and the other way around).
    {
        Wormhole wC;
        wC.reset(400.0f, 200.0f);
        for (int i = 0; i < 130; i++) wC.update(GAME_DT);
        Ship sC;
        sC.reset(340.0f, 200.0f); // d=60, left of the core
        sC.velX = sC.velY = 0.0f;
        CHECK(wC.apply(sC)); // captured: scripted vortex (one frame in)
        CHECK(wC.captured());
        CHECK(fabsf(sC.posX - 340.0f) < 20.0f);
        CHECK(fabsf(sC.posY - 200.0f) < 20.0f);
    }
    // Vertical case: a ship captured below the core must not jump upward to
    // the squashed side of the ellipse.
    {
        Wormhole wC2;
        wC2.reset(400.0f, 200.0f);
        for (int i = 0; i < 130; i++) wC2.update(GAME_DT);
        Ship sC2;
        sC2.reset(400.0f, 290.0f); // d=90, straight below the core
        sC2.velX = sC2.velY = 0.0f;
        CHECK(wC2.apply(sC2));
        CHECK(wC2.captured());
        CHECK(fabsf(sC2.posX - 400.0f) < 20.0f);
        CHECK(fabsf(sC2.posY - 290.0f) < 20.0f);
    }

    // CAPTURE BOUNDARY: d just above CAPTURE_R stays free; once inside the
    // ship is captured and no longer escapable even while thrusting away.
    {
        Wormhole wB1;
        wB1.reset(400.0f, 200.0f);
        for (int i = 0; i < 130; i++) wB1.update(GAME_DT);
        Ship sB1;
        sB1.reset(510.0f, 200.0f); // d=110 > CAPTURE_R
        sB1.velX = sB1.velY = 0.0f;
        wB1.apply(sB1);
        CHECK(!wB1.captured());
        CHECK(!wB1.swallowed());

        Wormhole wB2;
        wB2.reset(400.0f, 200.0f);
        for (int i = 0; i < 130; i++) wB2.update(GAME_DT);
        Ship sB2;
        sB2.reset(490.0f, 200.0f); // d=90 < CAPTURE_R
        sB2.velX = sB2.velY = 0.0f;
        wB2.apply(sB2);
        CHECK(wB2.captured());
        CHECK(!wB2.swallowed()); // captured but not yet swallowed
    }

    // Swallow happens only once the ship crosses the core rim (d < SWALLOW_R)
    // (or the vortex completes); inside the reach but outside the rim, a
    // single apply captures but does not swallow.
    {
        Wormhole wIn;
        wIn.reset(400.0f, 200.0f);
        for (int i = 0; i < 130; i++) wIn.update(GAME_DT);
        Ship sIn;
        sIn.reset(440.0f, 200.0f); // d=40 < CAPTURE_R: captured
        sIn.velX = sIn.velY = 0.0f;
        CHECK(wIn.apply(sIn)); // scripted vortex -> Game skips collisions
        CHECK(wIn.captured());
        CHECK(!wIn.swallowed());
    }
    {
        Wormhole wSw;
        wSw.reset(400.0f, 200.0f);
        for (int i = 0; i < 130; i++) wSw.update(GAME_DT);
        Ship sSw;
        sSw.reset(425.0f, 200.0f); // d=25 < SWALLOW_R
        sSw.velX = sSw.velY = 0.0f;
        CHECK(wSw.apply(sSw));
        CHECK(wSw.swallowed());
        CHECK(wSw.phase() == WH_SWALLOW);
        wSw.update(GAME_DT);
        for (int i = 0; i < 200; i++) wSw.update(GAME_DT); // SWALLOW -> DYING -> IDLE
        CHECK(wSw.phase() == WH_IDLE);
        CHECK(!wSw.active());
    }

    // Game integration: swallowing the ship waits for the hole to fade, then
    // fades the ship in on a random other moon between the sky and the terrain
    // (wormholeJump), and the fuel survives the warp.
    Game g;
    g.input.startPressed = true;
    g.update();
    g.input.startPressed = false;
    CHECK(g.state == STATE_PLAYING);
    g.level = 5; // CALLISTO (moonIndex 4): effect-free host
    g.wormhole.reset(400.0f, 110.0f);
    g.ship.reset(410.0f, 110.0f); // d=10 < SWALLOW_R: swallowed right away
    g.ship.fuel = 400.0f;
    g.fuel = 400.0f;
    int curMoon = moonIndex(g.level);
    bool seenJump = false;
    for (int i = 0; i < 1400 && !seenJump; i++) {
        g.update();
        if (g.state == STATE_PLAYING && moonIndex(g.level) != curMoon)
            seenJump = true;
    }
    CHECK(seenJump); // the teleport happened
    CHECK(g.state == STATE_PLAYING);
    CHECK(fabsf(g.ship.fuel - 400.0f) < 1.0f); // fuel preserved through the warp
    // Respawned between the sky and the terrain.
    CHECK(g.ship.posX >= 60.0f);
    float gy = g.terrain.yAt(g.ship.posX, 500.0f);
    CHECK(g.ship.posY >= 90.0f);
    CHECK(g.ship.posY < gy); // above the terrain at that x
    // The warp-in materialization runs during the intro and completes.
    CHECK(g.warpIn() > 0.0f);
    for (int i = 0; i < 300 && g.warpIn() > 0.0f; i++) g.update();
    CHECK(g.warpIn() == 0.0f);
    CHECK(g.ship.scale == 1.5f); // fully materialized
    // The "recycled" banner fires on the new moon and runs down to zero.
    CHECK(g.recycledBanner() > 0.0f);
    CHECK(g.recycledBanner() <= WORMHOLE_RECYCLED_T);
    for (int i = 0; i < (int)(WORMHOLE_RECYCLED_T / GAME_DT) + 2; i++) {
        g.wormhole.disable(); // no new warp on the new moon while the banner runs
        g.update();
    }
    CHECK(g.recycledBanner() == 0.0f);

    return 0;
}

static int testAcidRain()
{
    Terrain t;
    t.generate(3); // EUROPA

    // Activation: only on Europa, inactive on non-Europa moons.
    CHECK(moonHasAcidRain(3) == true);  // level 3 = EUROPA
    CHECK(moonHasAcidRain(11) == true); // level 11 = EUROPA
    CHECK(moonHasAcidRain(1) == false); // LUNA
    CHECK(moonHasAcidRain(2) == false); // IO
    AcidRain a;
    a.reset(3, t);
    CHECK(a.active());
    CHECK(a.cellCount() == ACID_RAIN_CELLS);
    CHECK(a.meterGet() == 0.0f);

    // Inactive on non-Europa moons.
    AcidRain a2;
    a2.reset(1, t); // LUNA
    CHECK(!a2.active());
    a2.reset(2, t); // IO
    CHECK(!a2.active());

    // SetEnabled forces off.
    a.setEnabled(false);
    CHECK(!a.active());

    // Meter corrodes inside rain and dries outside.
    AcidRain a3;
    a3.reset(3, t);
    for (int i = 0; i < (int)a3.cellCount(); i++) {
        float cx = a3.cellX(i);
        CHECK(a3.inRain(cx, 0.0f) == true);
    }
    CHECK(a3.inRain(-999.0f, 0.0f) == false);
    a3.corrode();
    CHECK(a3.meterGet() == ACID_RAIN_CORRODE);
    for (int i = 0; i < 50; i++) a3.corrode();
    CHECK(a3.meterGet() > 0.05f);
    for (int i = 0; i < 1000; i++) a3.dry();
    CHECK(a3.meterGet() == 0.0f);

    // Meter clamps at 100.
    for (int i = 0; i < 500000; i++) a3.corrode();
    CHECK(a3.meterGet() == 100.0f);

    // Acid burn flag via Game integration.
    Game g;
    g.input.startPressed = true;
    g.update();
    g.input.startPressed = false;
    CHECK(g.state == STATE_PLAYING);
    g.level = 3; // EUROPA
    g.newGame();
    CHECK(!g.acidBurnGet()); // newGame → level 1, no acid

    // Re-configure as Europa directly.
    g.terrain.generate(3);
    g.acidrain.reset(3, g.terrain);
    CHECK(g.acidrain.active());

    // Direct-tick the acid meter to 100 % without running the full game
    // loop (no collisions, no demo, no state transitions).
    while (g.acidrain.meterGet() < 100.0f) g.acidrain.corrode();
    CHECK(g.acidrain.meterGet() == 100.0f);

    // Now run one update() with the ship inside rain: it should detect
    // the 100 % meter and trigger the crash.
    float cx0 = g.acidrain.cellX(0);
    g.ship.posX = cx0;
    g.ship.posY = 80.0f;
    g.ship.velX = 0.0f;
    g.ship.velY = 0.0f;
    g.introTimer = 0.0f;
    g.wormhole.disable();
    g.update();
    CHECK(g.acidBurnGet());
    CHECK(g.state == STATE_CRASHED);

    // Reset clears the flag and the meter.
    g.acidrain.setEnabled(false);
    g.update();
    g.level = 3;
    g.newGame();
    CHECK(!g.acidBurnGet());
    CHECK(g.acidrain.meterGet() == 0.0f);

    return 0;
}

static int testQuake()
{
    // Activation: only on Io, inactive on other moons.
    CHECK(moonHasQuakes(2) == true);   // level 2 = IO
    CHECK(moonHasQuakes(10) == true);  // level 10 = IO
    CHECK(moonHasQuakes(1) == false);  // LUNA
    CHECK(moonHasQuakes(3) == false);  // EUROPA

    Terrain t;
    t.generate(2); // IO
    CHECK(t.zoneCount() == 4);

    Ship s;
    s.reset(200, 150);

    Quake q;
    q.reset(2, t, s);
    CHECK(q.active());
    CHECK(q.phase() == Quake::IDLE);

    // Inactive on non-Io moons.
    Quake q2;
    q2.reset(1, t, s); // LUNA
    CHECK(!q2.active());
    q2.reset(3, t, s); // EUROPA
    CHECK(!q2.active());

    // No zone ruptured yet.
    for (int i = 0; i < t.zoneCount(); i++) CHECK(!t.zoneBroken(i));

    // The strike follows the ship, so park it right over a landing pad (near
    // the surface) to force the pad-destruction showcase.
    int targetPad = 0;
    float p0x = t.zoneLabelX(targetPad);
    float p0y = t.getLines()[t.zoneStart(targetPad)].y1;
    s.posX = p0x;
    s.posY = p0y - 60.0f;

    // Drive the quake to completion (max wait 20s + rumble 1s).
    int guard = 0;
    while (q.phase() != Quake::BROKEN && guard < 500) {
        q.update(1.0f, t, s);
        guard++;
    }
    CHECK(q.phase() == Quake::BROKEN);
    CHECK(guard < 500);

    int rz = q.rupturedZone();
    CHECK(rz == targetPad);
    CHECK(t.zoneBroken(rz));

    // The ruptured pad is no longer landable: its segments are tilted and
    // the label (approach lights / minimap point) is gone.
    const std::vector<TerrainLine> &tl = t.getLines();
    int zs = t.zoneStart(rz);
    int zc = t.zoneSegCount(rz);
    CHECK(zs >= 0);
    for (int k = 0; k < zc; k++) {
        CHECK(!tl[zs + k].landable);
    }
    CHECK(tl[zs].labelX < 0);

    // A perfect approach on the ruptured pad still crashes.
    float rxc = t.zoneLabelX(rz);
    float baseY = tl[zs].y1;
    int res = t.checkLanding(rxc - 10.0f, rxc + 10.0f, baseY + QUAKE_LIFT + 20.0f,
                             0.0f, 0.05f, 0.0f);
    CHECK(res == 1);

    // isRupturedAt true inside the strike window (quake + persistent terrain).
    CHECK(q.isRupturedAt(q.strikeX()));
    CHECK(t.isRupturedAt(q.strikeX()));
    CHECK(!q.isRupturedAt(q.strikeX() + 5000.0f));

    // The other pads still land safely (only the struck pad broke).
    int safeZone = -1;
    for (int i = 0; i < t.zoneCount(); i++) {
        if (!t.zoneBroken(i)) { safeZone = i; break; }
    }
    CHECK(safeZone >= 0);
    int zs2 = t.zoneStart(safeZone);
    int zc2 = t.zoneSegCount(safeZone);
    float sx2 = t.zoneLabelX(safeZone);
    float zx1 = tl[zs2].x1;
    float zx2 = tl[zs2 + zc2 - 1].x2;
    float sy2 = tl[zs2].y1;
    CHECK(zx2 - zx1 > 20.0f);
    res = t.checkLanding(sx2 - 8.0f, sx2 + 8.0f, sy2 + 2.0f, 0.0f, 0.05f, 0.0f);
    CHECK(res == 2);

    // A regenerated level forgets the old ruptures.
    t.generate(2);
    CHECK(!t.isRupturedAt(q.strikeX()));
    for (int i = 0; i < t.zoneCount(); i++) CHECK(!t.zoneBroken(i));

    // Game integration: crashing on the ruptured pad sets the quake flag.
    Game g;
    g.input.startPressed = true;
    g.update();
    g.input.startPressed = false;
    g.level = 2; // IO
    g.terrain.generate(2);
    g.quake.reset(2, g.terrain, g.ship);
    g.introTimer = 0.0f;
    g.wormhole.disable();
    CHECK(!g.quakeCrashGet());
    CHECK(g.quake.active());

    // Park the ship over a landing pad (near the surface): the quake follows
    // the ship, so the next strike destroys that pad.
    int gPad = 0;
    float gPadX = g.terrain.zoneLabelX(gPad);
    g.ship.posX = gPadX;
    g.ship.posY = g.terrain.getLines()[g.terrain.zoneStart(gPad)].y1 - 60.0f;
    while (g.quake.phase() != Quake::BROKEN) g.quake.update(1.0f, g.terrain, g.ship);
    CHECK(g.quake.rupturedZone() == gPad);
    CHECK(g.quake.isRupturedAt(gPadX));
    CHECK(g.terrain.isRupturedAt(gPadX));

    // Crash the ship onto the ruptured pad.
    g.ship.posX = gPadX;
    int rzs = g.terrain.zoneStart(gPad);
    g.ship.posY = g.terrain.getLines()[rzs].y1 + QUAKE_LIFT - 5.0f;
    g.ship.velY = 0.05f;
    g.ship.velX = 0.0f;
    g.ship.rotation = 0.0f;
    g.ship.scale = 1.0f;
    g.ship.left = g.ship.posX - 10.0f * g.ship.scale;
    g.ship.right = g.ship.posX + 10.0f * g.ship.scale;
    g.ship.bottom = g.ship.posY + 14.0f * g.ship.scale;
    g.update();
    CHECK(g.quakeCrashGet());
    CHECK(g.state == STATE_CRASHED);

    // A clean landing elsewhere is not a quake crash.
    Game g2;
    g2.input.startPressed = true;
    g2.update();
    g2.input.startPressed = false;
    g2.level = 2;
    g2.newGame();
    g2.introTimer = 0.0f;
    g2.wormhole.disable();
    CHECK(!g2.quakeCrashGet());

    return 0;
}

static int testVolcanoRebuild()
{
    // After a quake buckles the surface, Volcanoes::rebuild() re-anchors each
    // crater on the new terrain and re-runs the flows so the lava ribbons
    // follow the post-quake shape instead of the pre-quake one.
    bool sawRupture = false;
    for (int s = 0; s < 60 && !sawRupture; s++) {
        srand(6000 + s);
        Terrain t;
        t.generate(2); // IO
        Volcanoes v;
        v.reset(2, t);
        CHECK(v.active());
        if (v.volcanoCount() == 0) continue;

        // Rupture right under the first volcano so its crater gets re-shaped.
        float cx = v.volcanoX(0);
        float gyBefore = v.volcanoGY(0);
        t.ruptureSurface(cx, QUAKE_SURFACE_HALF_W);
        float yNew = t.yAt(cx, 500.0f);
        if (fabsf(yNew - gyBefore) < 1.0f) continue; // barely moved: skip seed

        // Without a rebuild the crater still points at the old height.
        CHECK(fabsf(v.volcanoGY(0) - gyBefore) < 0.5f);
        CHECK(fabsf(yNew - gyBefore) > QUAKE_LIFT * 0.5f);

        v.rebuild(t);

        // Crater now sits on the new surface.
        CHECK(fabsf(v.volcanoGY(0) - t.yAt(cx, 500.0f)) < 0.5f);

        // Every flow sample lies on the current terrain (both arms).
        for (int arm = 0; arm < 2; arm++) {
            CHECK(v.flowLen(0, arm) >= 1);
            for (int k = 0; k < v.flowLen(0, arm); k++) {
                float fx = v.flowXAt(0, arm, k);
                float fy = v.flowYAt(0, arm, k);
                float ty = t.yAt(fx, fy);
                CHECK(fabsf(fy - ty) < 0.5f);
            }
        }
        sawRupture = true;
    }
    CHECK(sawRupture);
    return 0;
}

// Counting renderer: counts every primitive call (including off-screen ones)
// so a test can detect whether an effect's draw actually ran.
struct CountRenderer : public Renderer {
    long long px = 0;
    long long calls = 0;
    void clear() override { calls++; }
    void pixel(float, float) override { px++; calls++; }
    void pixelShade(float, float, int) override { px++; calls++; }
    void line(float, float, float, float) override { calls++; }
    void lineShade(float, float, float, float, int) override { px++; calls++; }
    void rect(float, float, float, float) override { calls++; }
    void rectShade(float, float, float, float, int) override { px++; calls++; }
    void circle(float, float, float) override { calls++; }
    void text(float, float, const char*) override { calls++; }
    void textScaled(float, float, const char*, float, int) override { calls++; }
    void fillPolygon(const float*, const float*, int, int) override { px++; calls++; }
    void setClip(float, float, float, float) override { calls++; }
    void clearClip() override { calls++; }
    void flush() override { calls++; }
    int width() const override { return (int)SCREEN_W; }
    int height() const override { return (int)SCREEN_H; }
};

static int testViewportCull()
{
    // In a zoomed view (5x) the wormhole must be neither updated nor drawn
    // while its core sits outside the visible world rectangle: park the ship
    // low over the terrain so the camera zooms in, and place the hole in the
    // sky above the viewport.
    Game g;
    g.input.startPressed = true;
    g.update();
    g.input.startPressed = false;
    g.newGame();
    g.introTimer = 0.0f;
    float gy = g.terrain.yAt(400.0f, 500.0f);
    g.ship.posX = 400.0f;
    g.ship.posY = gy - 40.0f;
    g.ship.velX = 0.0f;
    g.ship.velY = 0.0f;
    g.ship.rotation = 0.0f;
    g.ship.targetRotation = 0.0f;

    // Update culling: an EMERGING hole just below the ACTIVE threshold must
    // not advance while off screen. The 1st update engages the zoom (still
    // normal view this tick), the 2nd runs fully zoomed with the hole culled.
    g.wormhole.reset(400.0f, WORMHOLE_SKY_Y_MIN); // far above the zoom view
    for (int i = 0; i < (int)(WORMHOLE_EMERGE_T / GAME_DT) - 1; i++)
        g.wormhole.update(GAME_DT);
    CHECK(g.wormhole.phase() == WH_EMERGING);
    g.update();
    g.update();
    CHECK(g.wormhole.phase() == WH_EMERGING); // frozen: t_ still < EMERGE_T

    // Control: the same hole parked in the zoomed viewport near the ship (but
    // beyond WORMHOLE_SWALLOW_R and WORMHOLE_CAPTURE_R so the radial pull does
    // not script it) advances EMERGING -> ACTIVE in those two updates.
    g.wormhole.reset(306.0f, gy + 40.0f);
    for (int i = 0; i < (int)(WORMHOLE_EMERGE_T / GAME_DT) - 1; i++)
        g.wormhole.update(GAME_DT);
    CHECK(g.wormhole.phase() == WH_EMERGING);
    g.update();
    g.update();
    CHECK(g.wormhole.phase() == WH_ACTIVE);

    // Draw culling: same scene, hole ACTIVE off screen vs on screen. The only
    // difference is the wormhole, so the extra pixels prove the off-screen
    // draw was skipped (and the on-screen one ran).
    g.wormhole.reset(400.0f, WORMHOLE_SKY_Y_MIN);
    for (int i = 0; i < (int)(WORMHOLE_EMERGE_T / GAME_DT) + 10; i++)
        g.wormhole.update(GAME_DT);
    CHECK(g.wormhole.phase() == WH_ACTIVE);
    CountRenderer off;
    g.draw(off);
    g.wormhole.reset(400.0f, gy - 30.0f);
    for (int i = 0; i < (int)(WORMHOLE_EMERGE_T / GAME_DT) + 10; i++)
        g.wormhole.update(GAME_DT);
    CountRenderer on;
    g.draw(on);
    CHECK(on.px > off.px);
    CHECK(off.calls > 0);
    CHECK(on.calls > 0);
    return 0;
}

int main()
{
    int r;
    r = testShip();
    if (r) return r;
    r = testTerrain();
    if (r) return r;
    r = testGame();
    if (r) return r;
    r = testLevels();
    if (r) return r;
    r = testLandingBonus();
    if (r) return r;
    r = testDemo();
    if (r) return r;
    r = testStorm();
    if (r) return r;
    r = testMoon();
    if (r) return r;
    r = testGeysers();
    if (r) return r;
    r = testVolcanoes();
    if (r) return r;
    r = testVolcanoLava();
    if (r) return r;
    r = testVolcanoRebuild();
    if (r) return r;
    r = testAtmosphere();
    if (r) return r;
    r = testRings();
    if (r) return r;
    r = testTwister();
    if (r) return r;
    r = testWormhole();
    if (r) return r;
    r = testAcidRain();
    if (r) return r;
    r = testQuake();
    if (r) return r;
    r = testTanker();
    if (r) return r;
    r = testParachute();
    if (r) return r;
    r = testViewportCull();
    if (r) return r;
    printf("ALL CHECKS PASSED (%d)\n", checks);
    return 0;
}
