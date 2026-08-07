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
    for (int i = 0; i < 200000; i++) {
        g.update();
        if (g.state == STATE_LANDED || g.state == STATE_CRASHED) outcomeSeen = true;
        if (g.state == STATE_WAITING) {
            finished = true;
            break;
        }
    }
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
    CHECK(r.rocksInRing(0) == RING_SMALL_HIGH + RING_DANGER_HIGH);
    CHECK(r.rocksInRing(1) == RING_SMALL_LOW + RING_DANGER_LOW);
    CHECK(r.rockDanger(0, 0) == false);
    CHECK(r.rockDanger(0, RING_SMALL_HIGH) == true);
    CHECK(r.rockDanger(0, RING_SMALL_HIGH + RING_DANGER_HIGH) == false);
    r.reset(1, t);
    CHECK(!r.active());

    r.reset(4, t);
    // Some rock must be visible above the terrain and another reachable to hit.
    float wx = 0, wy = 0;
    bool sawVisible = false;
    for (int k = 0; k < RING_COUNT; k++) {
        for (int i = 0; i < r.rocksInRing(k); i++) {
            if (r.rockVisible(t, k, i, wx, wy)) { sawVisible = true; break; }
        }
        if (sawVisible) break;
    }
    CHECK(sawVisible);
    // Park the ship exactly on the first DANGER rock -> guaranteed hit.
    float rx = 0, ry = 0;
    for (int k = 0; k < RING_COUNT && ry == 0.0f && rx == 0.0f; k++) {
        for (int i = 0; i < r.rocksInRing(k); i++) {
            if (r.rockVisible(t, k, i, rx, ry) && r.rockDanger(k, i)) break;
        }
    }
    CHECK(r.hitsShip(t, rx, ry, RING_SHIP_RADIUS));
    // Far above the highest possible band (terrain top - RING_HEIGHT_HIGH) ->
    // never hit.
    CHECK(!r.hitsShip(t, 400.0f, 10.0f, RING_SHIP_RADIUS));

    // Rings move over time (positions are a function of the advancing phase).
    float p0x = 0, p0y = 0, p1x = 999, p1y = 999;
    bool vis0 = false, vis1 = false;
    for (int k = 0; k < RING_COUNT && !vis0; k++)
        for (int i = 0; i < r.rocksInRing(k); i++)
            if (r.rockVisible(t, k, i, p0x, p0y)) { vis0 = true; break; }
    CHECK(vis0);
    for (int i = 0; i < 2000; i++) r.update(GAME_DT);
    for (int k = 0; k < RING_COUNT && !vis1; k++)
        for (int i = 0; i < r.rocksInRing(k); i++)
            if (r.rockVisible(t, k, i, p1x, p1y)) { vis1 = true; break; }
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
    r = testAtmosphere();
    if (r) return r;
    r = testRings();
    if (r) return r;
    r = testTwister();
    if (r) return r;
    printf("ALL CHECKS PASSED (%d)\n", checks);
    return 0;
}
