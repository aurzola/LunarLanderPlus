#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include "ship.h"
#include "terrain.h"
#include "game.h"
#include "config.h"

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
    CHECK(g.level == 1);
    CHECK(g.introTimer > 0);

    for (int i = 0; i < (int)(LEVEL_INTRO_TIME / GAME_DT) + 1; i++) g.update();
    CHECK(g.introTimer == 0);

    g.ship.fuel = 50.0f;
    g.state = STATE_LANDED;
    g.update();
    CHECK(g.level == 2);
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
    printf("ALL CHECKS PASSED (%d)\n", checks);
    return 0;
}
