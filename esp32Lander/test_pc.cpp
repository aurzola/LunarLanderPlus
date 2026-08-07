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

    t.generate(3);
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
    printf("ALL CHECKS PASSED (%d)\n", checks);
    return 0;
}
