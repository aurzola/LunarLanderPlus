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

int main()
{
    int r;
    r = testShip();
    if (r) return r;
    r = testTerrain();
    if (r) return r;
    r = testGame();
    if (r) return r;
    printf("ALL CHECKS PASSED (%d)\n", checks);
    return 0;
}
