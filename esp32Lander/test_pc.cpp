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
    CHECK(s.getGas() == 750);
    s.setPos(100, 100);
    s.setVel(50, 0);
    CHECK(s.getXpos() == 100 && s.getYpos() == 100);
    CHECK(s.getXvel() == 50 && s.getYvel() == 0);

    s.setGas(10);
    s.setGas(-5);
    CHECK(s.getGas() == 0);
    s.setGas(100);
    CHECK(s.getGas() == 100);

    s.setAng(0.5);
    s.rotate(0.5);
    CHECK(s.getAng() == 0);
    s.rotate(-4.0);
    CHECK(fabs(s.getAng() + PI) < 1e-9);
    s.rotate(-2.0);
    CHECK(fabs(s.getAng() + 2.0) < 1e-9);

    s.setAng(-1.0);
    s.setAccMode(8);
    double xv = 0, yv = 0;
    double before = s.getGas();
    s.accelerate(xv, yv);
    CHECK(s.getGas() < before);
    CHECK(s.getGas() >= 0);
    CHECK(yv < 0);

    s.setAccMode(8);
    s.accelerateChange(1);
    CHECK(s.getAccMode() == 8);
    s.accelerateChange(-1);
    CHECK(s.getAccMode() == 7);

    s.setGas(0);
    s.setAccMode(3);
    s.accelerateChange(1);
    CHECK(s.getAccMode() == 0);
    return 0;
}

static int testShipCollision()
{
    const double kx = SCREEN_W / WORLD_W;
    const double ky = SCREEN_H / WORLD_H;

    std::vector<int> xt, yt;
    for (int x = 0; x <= 300; x++) {
        xt.push_back(x);
        yt.push_back(100);
    }

    double sx = 150.0 * kx;
    double syTerrain = 100.0 * ky;

    Ship crash;
    crash.setPos(sx / kx, syTerrain / ky);
    crash.setAng(-PI / 2.0);
    crash.hitbox();
    CHECK(crash.collision(xt, yt) == 1);

    Ship land;
    land.setPos(sx / kx, (syTerrain - 10.39f) / ky);
    land.setAng(-PI / 2.0);
    land.hitbox();
    CHECK(land.collision(xt, yt) == 2);

    Ship safe;
    safe.setPos(sx / kx, (syTerrain - 30.0) / ky);
    safe.setAng(-PI / 2.0);
    safe.hitbox();
    CHECK(safe.collision(xt, yt) == 0);
    return 0;
}

static int testTerrain()
{
    Terrain t;
    t.generate(1400, 800, 450, 1);
    std::vector<int> xs = t.getXPoints();
    std::vector<int> ys = t.getYPoints();
    CHECK(xs.size() == ys.size());
    CHECK(xs.size() > 1300 && xs.size() <= 1450);
    for (size_t i = 1; i < xs.size(); i++) CHECK(xs[i] >= xs[i - 1]);
    for (size_t i = 0; i < xs.size(); i++) {
        CHECK(ys[i] >= 0 && ys[i] <= 800);
    }
    int m = t.multiplierCheck(-10);
    CHECK(m >= 1 && m <= 5);
    return 0;
}

static int testGame()
{
    Game g;
    g.update();
    CHECK(g.state == STATE_MENU);

    g.input.startPressed = true;
    g.update();
    g.input.startPressed = false;
    CHECK(g.state == STATE_PLAYING);
    CHECK(g.ship.getGas() == 750);
    CHECK(g.ship.getXpos() == 100 && g.ship.getYpos() == 100);

    int i;
    for (i = 0; i < 3000 && g.playing; i++) {
        g.input.angle = -0.3f;
        g.input.throttle = 0;
        g.update();
    }
    CHECK(!g.playing);
    CHECK(g.collided == 1 || g.collided == 2);
    CHECK(g.score == 5);
    CHECK(g.ship.getGas() == 650);

    for (i = 0; i < 410; i++) g.update();
    CHECK(g.playing);
    CHECK(g.state == STATE_PLAYING);

    g.ship.setGas(30);
    for (i = 0; i < 3000 && g.state == STATE_PLAYING; i++) {
        g.input.angle = 0.0f;
        g.input.throttle = 0;
        g.update();
    }
    CHECK(g.state == STATE_GAMEOVER);
    CHECK(g.ship.getGas() == 0);

    for (i = 0; i < 510; i++) g.update();
    CHECK(g.state == STATE_MENU);
    return 0;
}

int main()
{
    int r;
    r = testShip();
    if (r) return r;
    r = testShipCollision();
    if (r) return r;
    r = testTerrain();
    if (r) return r;
    r = testGame();
    if (r) return r;
    printf("ALL CHECKS PASSED (%d)\n", checks);
    return 0;
}
