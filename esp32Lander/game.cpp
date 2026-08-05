#include <cmath>
#include <cstdio>
#include <cstring>
#include "game.h"

static const int TITLE_STAR_COUNT = 32;
static const int titleStars[][2] = {
    { 293,199 }, { 283,19 }, { 188,90 }, { 169,29 }, { 62,94 }, { 10,20 },
    { 226,90 }, { 163,155 }, { 115,14 }, { 249,125 }, { 209,131 }, { 308,77 },
    { 204,40 }, { 259,43 }, { 14,59 }, { 106,79 }, { 83,24 }, { 174,193 },
    { 55,205 }, { 20,89 }, { 9,170 }, { 218,205 }, { 256,199 }, { 110,188 },
    { 220,175 }, { 36,133 }, { 107,118 }, { 150,96 }, { 228,3 }, { 66,144 },
    { 274,106 }, { 82,60 },
};

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

Game::Game()
    : state(STATE_WAITING), score(0), level(1), fuel(FUEL_MAX), introTimer(0),
      demo(false), demoTimer(DEMO_START_DELAY),
      viewX(0), viewY(0), viewScale(1.0f),
      zoomedIn(false), resetTimer(0), landMultiplier(1),
      demoSkill(1.0f), demoTargetX(0), demoTargetY(0)
{
    input.startPressed = false;
    input.angle = 0;
    input.thrust = 0;
    input.powerLevel = 0;
    terrain.init();
    setZoom(false);
    setupTitleShip();
}

void Game::newGame()
{
    level = 1;
    score = 0;
    fuel = FUEL_MAX;
    ship.fuel = FUEL_MAX;
    state = STATE_PLAYING;
    ship.reset(110, 150);
    setZoom(false);
    resetTimer = 0;
    introTimer = LEVEL_INTRO_TIME;
    ship.velX = 0.415f;
    terrain.init();
}

void Game::restartLevel()
{
    float f = ship.fuel;
    ship.reset(110, 150);
    ship.fuel = f;
    setZoom(false);
    resetTimer = 0;
    introTimer = LEVEL_INTRO_TIME;

    if (state == STATE_GAMEOVER || state == STATE_WAITING) {
        state = STATE_WAITING;
        setupTitleShip();
    } else {
        state = STATE_PLAYING;
    }
}

void Game::nextLevel()
{
    level++;
    float f = ship.fuel;
    terrain.generate(level);
    state = STATE_PLAYING;
    ship.reset(110, 150);
    ship.fuel = f;
    setZoom(false);
    resetTimer = 0;
    introTimer = LEVEL_INTRO_TIME;
    ship.velX = 0.415f;
}

void Game::endGame()
{
    state = STATE_GAMEOVER;
    resetTimer = GAMEOVER_RESET_DELAY;
}

void Game::startDemo()
{
    demo = true;
    if (rand() % 100 < 50) demoSkill = (float)(rand() % 36) / 100.0f;
    else demoSkill = 0.6f + (float)(rand() % 41) / 100.0f;
    level = 1 + rand() % DEMO_MAX_LEVEL;
    score = 0;
    fuel = FUEL_MAX;
    ship.fuel = FUEL_MAX;
    state = STATE_PLAYING;
    if (level <= 1) terrain.init();
    else terrain.generate(level);
    ship.reset(110, 150);
    ship.velX = 0.06f;
    setZoom(false);
    resetTimer = 0;
    introTimer = LEVEL_INTRO_TIME;

    const std::vector<TerrainLine> &tl = terrain.getLines();
    std::vector<float> cx, cy;
    for (int i = 0; i < (int)tl.size(); i++) {
        if (tl[i].labelX >= 0) {
            cx.push_back(tl[i].labelX);
            cy.push_back(tl[i].y1);
        }
    }
    int pick = (int)cx.size() ? rand() % (int)cx.size() : 0;
    demoTargetX = cx[pick];
    demoTargetY = cy[pick];
    if (demoSkill < 0.35f) {
        float off = ((float)(rand() % 200) / 100.0f - 1.0f) * (0.35f - demoSkill) * 110.0f;
        demoTargetX += off;
    }
}

void Game::endDemoToTitle()
{
    state = STATE_WAITING;
    demoTimer = DEMO_START_DELAY;
    terrain.init();
    setZoom(false);
    setupTitleShip();
}

void Game::setupTitleShip()
{
    ship.reset(110, 150);
    ship.velX = -0.35f;
    ship.posX = (SCREEN_W - 20.0f) / viewScale;
}

void Game::runDemoAI()
{
    float errX = demoTargetX - ship.posX;
    float distX = fabsf(errX);
    float alt = ship.altitude;

    float desVX = clampf(errX * 0.003f, -0.10f, 0.10f);
    if (distX < 40.0f) desVX = clampf(errX * 0.002f, -0.04f, 0.04f);
    float want = clampf((desVX - ship.velX) / THRUST_ACCEL, -1.0f, 1.0f);
    float angle = asinf(want) * 180.0f / PI;
    float thrust = fabsf(want) * 0.8f;

    if (distX > 50.0f) {
        float maxVY = (alt < 100.0f) ? 0.04f : 0.12f;
        if (ship.velY > maxVY) {
            angle *= 0.4f;
            thrust = 1.0f;
        }
        if (alt < 60.0f) {
            angle *= 0.2f;
            thrust = 1.0f;
        }
    } else {
        if (ship.velY > 0.075f) {
            angle *= 0.3f;
            thrust = 1.0f;
        }
        if (alt < 12.0f) angle *= 0.4f;
    }

    float imp = 1.0f - demoSkill;
    float n = (float)(rand() % 1001) / 1000.0f - 0.5f;
    angle += n * imp * 40.0f;
    thrust = clampf(thrust + n * imp * 0.25f, 0.0f, 1.0f);

    input.angle = clampf(angle, -90.0f, 90.0f) * (PI / 180.0f);
    input.thrust = thrust;
    float pw = input.powerLevel;
    float step = DEMO_POWER_RATE * GAME_DT;
    if (thrust > pw) pw = fminf(thrust, pw + step);
    else pw = fmaxf(thrust, pw - step);
    input.powerLevel = pw;
}

void Game::setZoom(bool zoom)
{
    if (zoom) {
        viewScale = SCREEN_H / 700.0f * 5.0f;
        zoomedIn = true;
        viewX = -ship.posX * viewScale + SCREEN_W / 2.0f;
        viewY = -ship.posY * viewScale + SCREEN_H * 0.25f;
        ship.scale = 0.32f;
    } else {
        viewScale = SCREEN_H / 700.0f;
        zoomedIn = false;
        ship.scale = 1.0f;
        viewX = 0;
        viewY = 0;
    }
}

void Game::updateView()
{
    float margintop = SCREEN_H * 0.2f;
    float marginbottom = SCREEN_H * 0.3f;
    float marginx = SCREEN_W * 0.2f;

    if (!zoomedIn && ship.altitude < ZOOM_IN_ALT) {
        setZoom(true);
    } else if (zoomedIn && ship.altitude > ZOOM_OUT_ALT) {
        setZoom(false);
    }

    float sx = ship.posX * viewScale + viewX;
    if (sx < marginx) viewX = -ship.posX * viewScale + marginx;
    else if (sx > SCREEN_W - marginx) viewX = -ship.posX * viewScale + SCREEN_W - marginx;

    float sy = ship.posY * viewScale + viewY;
    if (sy < margintop) viewY = -ship.posY * viewScale + margintop;
    else if (sy > SCREEN_H - marginbottom) viewY = -ship.posY * viewScale + SCREEN_H - marginbottom;
}

void Game::checkCollisions()
{
    int result = terrain.checkLanding(
        ship.left, ship.right, ship.bottom,
        ship.rotation, ship.velY, ship.velX
    );

    if (result == 2) {
        float mult = 1.0f;
        for (int i = 0; i < (int)terrain.getLines().size(); i++) {
            const TerrainLine &l = terrain.getLines()[i];
            if (ship.posX >= l.x1 && ship.posX <= l.x2 && l.multiplier > 1) {
                mult = (float)l.multiplier;
                break;
            }
        }

        ship.land();
        if (ship.velY < LAND_PERFECT_VY) {
            score += (int)(50 * mult);
            fuel += 50;
            ship.fuel += 50;
            if (fuel > FUEL_MAX) fuel = FUEL_MAX;
            if (ship.fuel > FUEL_MAX) ship.fuel = FUEL_MAX;
        } else {
            score += (int)(15 * mult);
        }
        state = STATE_LANDED;
        resetTimer = CRASH_RESET_DELAY;

    } else if (result == 1) {
        int lost = 200 + (rand() % 200);
        ship.crash();
        fuel -= lost;
        ship.fuel -= lost;
        if (ship.fuel < 0) ship.fuel = 0;
        if (fuel < 0) fuel = 0;
        score += 5;
        state = STATE_CRASHED;
        resetTimer = CRASH_RESET_DELAY;
    }
}

void Game::update()
{
    float dt = GAME_DT;

    if (input.startPressed && demo) {
        demo = false;
        newGame();
        return;
    }

    if (state == STATE_WAITING) {
        ship.update();
        ship.altitude = terrain.getLines()[0].y1 - ship.bottom;
        if (input.startPressed) {
            newGame();
        } else {
            demoTimer -= dt;
            if (demoTimer <= 0) startDemo();
        }
        return;
    }

    if (state == STATE_PLAYING) {
        if (introTimer > 0) {
            introTimer -= dt;
            if (introTimer < 0) introTimer = 0;
            return;
        }

        if (demo) runDemoAI();

        float deg = input.angle * 180.0f / PI;
        ship.setTargetRotation(deg);
        ship.setThrust(input.thrust);
        ship.update();

        if (ship.posX > terrain.getWidth() + 10)
            ship.posX = -10;
        else if (ship.posX < -10)
            ship.posX = terrain.getWidth() + 10;

        if (ship.posY < -50) {
            ship.reset(110, 150);
            ship.velX = 2;
        }

        float minAlt = 9999;
        for (int i = 0; i < (int)terrain.getLines().size(); i++) {
            const TerrainLine &l = terrain.getLines()[i];
            if (ship.posX >= l.x1 && ship.posX <= l.x2) {
                float alt = l.y1 - ship.bottom;
                if (alt < minAlt) minAlt = alt;
            }
        }
        ship.altitude = minAlt;

        updateView();
        checkCollisions();
        return;
    }

    if (state == STATE_LANDED || state == STATE_CRASHED) {
        ship.update();
        resetTimer -= dt;
        if (resetTimer <= 0) {
            if (demo) {
                endDemoToTitle();
            } else if (ship.fuel <= 0) {
                endGame();
            } else if (state == STATE_LANDED) {
                nextLevel();
            } else {
                restartLevel();
            }
        }
        return;
    }

    if (state == STATE_GAMEOVER) {
        ship.update();
        resetTimer -= dt;
        if (resetTimer <= 0) {
            state = STATE_WAITING;
            demo = false;
            demoTimer = DEMO_START_DELAY;
            setZoom(false);
            setupTitleShip();
        }
    }
}

void Game::draw(Renderer &r)
{
    r.clear();

    if (state == STATE_WAITING) {
        for (int i = 0; i < TITLE_STAR_COUNT; i++) {
            r.rect((float)titleStars[i][0], (float)titleStars[i][1], 1.0f, 1.0f);
        }
        terrain.draw(r, viewX, viewY, viewScale, ship.counter, false);
        ship.draw(r, viewX, viewY, viewScale);

        r.textScaled(70, 20, "LUNAR LANDER++", 2.0f, 255);

        if ((ship.counter % 50) < 30) r.text(103, 183, "PRESS BUTTON TO PLAY");

        auto SX = [](float x) { return x * 1.2f + 26.0f; };
        auto SY = [](float y) { return y * 1.2f + 56.0f; };

        // Descent stage: octagonal base and landing legs.
        r.line(SX(40), SY(55), SX(60), SY(55));
        r.line(SX(40), SY(55), SX(30), SY(80));
        r.line(SX(60), SY(55), SX(70), SY(80));
        r.line(SX(30), SY(80), SX(70), SY(80));

        r.line(SX(50), SY(80), SX(50), SY(95));
        r.circle(SX(50), SY(95), 2.4f);
        r.line(SX(30), SY(80), SX(15), SY(90));
        r.line(SX(15), SY(90), SX(5), SY(95));
        r.circle(SX(5), SY(95), 1.8f);
        r.line(SX(70), SY(80), SX(85), SY(90));
        r.line(SX(85), SY(90), SX(95), SY(95));
        r.circle(SX(95), SY(95), 1.8f);

        r.rect(SX(48), SY(55), 4.8f, 30.0f);
        r.line(SX(48), SY(57), SX(52), SY(57));
        r.line(SX(48), SY(61), SX(52), SY(61));
        r.line(SX(48), SY(65), SX(52), SY(65));
        r.line(SX(48), SY(69), SX(52), SY(69));
        r.line(SX(48), SY(73), SX(52), SY(73));
        r.line(SX(48), SY(77), SX(52), SY(77));

        for (int y = 56; y < 79; y += 2) {
            for (float x = 40 + (y - 56) * 0.5f; x < 60 - (y - 56) * 0.5f; x += 3) {
                r.pixel(SX(x), SY((float)y));
            }
        }
        for (int y = 56; y < 79; y += 1) {
            r.pixel(SX(38 - (y - 56) * 0.3f), SY((float)y));
            r.pixel(SX(37 - (y - 56) * 0.3f), SY((float)y));
            r.pixel(SX(62 + (y - 56) * 0.3f), SY((float)y));
            r.pixel(SX(63 + (y - 56) * 0.3f), SY((float)y));
        }

        // Ascent stage: body, central panel and side boxes.
        r.line(SX(35), SY(55), SX(35), SY(30));
        r.line(SX(65), SY(55), SX(65), SY(30));
        r.line(SX(35), SY(30), SX(45), SY(15));
        r.line(SX(65), SY(30), SX(55), SY(15));
        r.line(SX(45), SY(15), SX(55), SY(15));

        r.line(SX(43), SY(30), SX(57), SY(30));
        r.line(SX(43), SY(30), SX(40), SY(50));
        r.line(SX(57), SY(30), SX(60), SY(50));
        r.line(SX(40), SY(50), SX(60), SY(50));
        r.rect(SX(48), SY(35), 4.8f, 12.0f);

        r.rect(SX(28), SY(38), 8.4f, 9.6f);
        r.line(SX(28), SY(38), SX(26), SY(40));
        r.rect(SX(65), SY(38), 8.4f, 9.6f);
        r.line(SX(72), SY(38), SX(74), SY(40));

        for (int y = 16; y < 30; y += 2) {
            r.line(SX(45 - (y - 15) * 0.1f), SY((float)y), SX(46 - (y - 15) * 0.1f), SY((float)y));
            r.line(SX(54 + (y - 15) * 0.1f), SY((float)y), SX(55 + (y - 15) * 0.1f), SY((float)y));
        }
        for (int y = 30; y < 50; y += 1) {
            if (y % 3 == 0) {
                r.line(SX(36), SY((float)y), SX(39), SY((float)y));
                r.line(SX(61), SY((float)y), SX(64), SY((float)y));
            }
        }
        for (int y = 31; y < 49; y += 2) {
            for (int x = 40; x < 60; x += 2) {
                if ((x + y) % 4 == 0) r.pixel(SX((float)x), SY((float)y));
            }
        }

        // Tracking dish antenna (left).
        r.line(SX(32), SY(28), SX(25), SY(23));
        r.line(SX(25), SY(23), SX(23), SY(26));
        r.rect(SX(21), SY(23), 4.8f, 3.6f);
        r.circle(SX(23), SY(23), 3.6f);
        r.circle(SX(23), SY(23), 1.8f);

        // Omnidirectional antenna (top).
        r.line(SX(50), SY(15), SX(50), SY(2));
        r.circle(SX(50), SY(10), 3.6f);
        r.line(SX(47), SY(10), SX(53), SY(10));
        r.line(SX(50), SY(7), SX(50), SY(13));
        r.line(SX(50), SY(10), SX(50), SY(8));
        r.pixel(SX(50), SY(8));
        for (float a = 0; a < 6.2832f; a += 0.3491f) {
            r.pixel(SX(50 + cosf(a) * 2.5f), SY(10 + sinf(a) * 2.5f));
        }

        // RCS thrusters.
        for (int i = 0; i < 4; ++i) {
            float ox = 45 + i * 3.3f;
            float oy = 25;
            r.circle(SX(ox), SY(oy), 1.2f);
            r.pixel(SX(ox), SY(oy - 1));
            r.pixel(SX(ox), SY(oy + 1));
            r.pixel(SX(ox - 1), SY(oy));
            r.pixel(SX(ox + 1), SY(oy));
        }
        r.circle(SX(30), SY(35), 1.2f);
        r.line(SX(30), SY(34), SX(30), SY(36));
        r.circle(SX(70), SY(35), 1.2f);
        r.line(SX(70), SY(34), SX(70), SY(36));

        // Helical antenna (right).
        r.line(SX(62), SY(25), SX(68), SY(20));
        r.rect(SX(67), SY(18), 1.2f, 4.8f);
        for (int y = 18; y < 22; y += 1) {
            r.pixel(SX(68), SY((float)y));
        }

        // Ground shadows under the footpads.
        for (int x = 0; x < 10; ++x) {
            if (x % 3 == 0) r.line(SX((float)x), SY(96), SX((float)x + 1), SY(96));
        }
        for (int x = 45; x < 55; ++x) {
            if (x % 3 == 0) r.line(SX((float)x), SY(97), SX((float)x + 1), SY(97));
        }
        for (int x = 90; x < 100; ++x) {
            if (x % 3 == 0) r.line(SX((float)x), SY(96), SX((float)x + 1), SY(96));
        }

        r.text(170, 132, "STICK: ROTATION");
        r.text(170, 144, "Z: ENGINE ON/OFF");
        r.text(170, 156, "C: POWER STEPS");
        r.text(170, 168, "POT: POWER LEVEL");

        r.text(40, 232, "COPYRIGHT ALEX URZOLA 2026/OPENCODE");
    } else {
        terrain.draw(r, viewX, viewY, viewScale, ship.counter);
        ship.draw(r, viewX, viewY, viewScale);

        {
            const std::vector<TerrainLine> &tl = terrain.getLines();
            int blink = (ship.counter / 20) & 1;
            for (int i = 0; i < (int)tl.size(); i++) {
                if (tl[i].labelX < 0) continue;
                float zx1 = tl[i].x1, zx2 = tl[i].x2;
                int j = i;
                while (j + 1 < (int)tl.size() && tl[j + 1].landable) {
                    j++;
                    zx2 = tl[j].x2;
                }
                float zy = tl[i].y1 + 3.0f;
                float zw = zx2 - zx1;
                int n = (int)(zw / 5.0f);
                if (n < 3) n = 3;
                if (n > 12) n = 12;
                for (int k = 0; k < n; k++) {
                    if (((k + blink) & 1) == 0) continue;
                    float wx = zx1 + zw * (k + 0.5f) / n;
                    r.rect(wx * viewScale + viewX - 1.0f,
                           zy * viewScale + viewY - 1.0f, 2.0f, 2.0f);
                }
                i = j;
            }
        }

        char buf[40];
        if (introTimer <= 0) {
            snprintf(buf, sizeof buf, "SCORE %d", score);
            r.text(22, 22, buf);
            snprintf(buf, sizeof buf, "FUEL %d", (int)ship.fuel);
            r.text(22, 32, buf);
            snprintf(buf, sizeof buf, "ANG %d", (int)ship.rotation);
            r.text(22, 42, buf);
            snprintf(buf, sizeof buf, "PWR %d", (int)(input.powerLevel * 100));
            r.text(22, 52, buf);

            int alt = (ship.altitude < 0) ? 0 : (int)ship.altitude;
            snprintf(buf, sizeof buf, "ALT %d", alt);
            r.text(250, 22, buf);
            snprintf(buf, sizeof buf, "VX %d", (int)(ship.velX * 200));
            r.text(250, 32, buf);
            snprintf(buf, sizeof buf, "VY %d", (int)(ship.velY * 200));
            r.text(250, 42, buf);

            if (demo) r.text(22, 62, "DEMO");
        }

        if (state == STATE_LANDED) {
            if (ship.velY < LAND_PERFECT_VY) {
                r.text(60, 90, "CONGRATULATIONS");
                r.text(64, 102, "PERFECT LANDING");
            } else {
                r.text(72, 90, "HARD LANDING");
                r.text(60, 102, "HOPELESSLY MAROONED");
            }
        } else if (state == STATE_CRASHED) {
            r.text(72, 90, "YOU CRASHED");
            r.text(48, 102, "FUEL TANKS DESTROYED");
        } else if (state == STATE_GAMEOVER) {
            r.text(72, 90, "OUT OF FUEL");
            r.text(96, 102, "GAME OVER");
        }

        if (state == STATE_PLAYING && introTimer <= 0) {
            if (ship.fuel <= 0) {
                if ((ship.counter % 50) < 30) r.text(250, 52, "OUT OF FUEL");
            } else if (ship.fuel < 300) {
                if ((ship.counter % 50) < 30) r.text(250, 52, "LOW FUEL");
            }
            if ((ship.velY > LAND_HARD_VY ||
                 ship.velX > LAND_HARD_VX || ship.velX < -LAND_HARD_VX) &&
                (ship.counter % 50) < 30) {
                r.text(250, 62, "TOO FAST");
            }
        }

        if (introTimer > 0) {
            float elapsed = LEVEL_INTRO_TIME - introTimer;
            float b = 1.0f;
            if (elapsed < INTRO_FADE_IN) b = elapsed / INTRO_FADE_IN;
            else if (introTimer < INTRO_FADE_OUT) b = introTimer / INTRO_FADE_OUT;
            int brightness = (int)(b * 255.0f);
            int scale = 3;
            snprintf(buf, sizeof buf, "LEVEL %d", level);
            int tw = (int)strlen(buf) * 6 * scale;
            float tx = (SCREEN_W - tw) / 2.0f;
            float ty = (SCREEN_H - 7 * scale) / 2.0f;
            r.textScaled(tx, ty, buf, (float)scale, brightness);
        }

        if (zoomedIn) {
            const float MX = 112, MY = 22, MW = 96, MH = 49;
            r.line(MX, MY, MX + MW, MY);
            r.line(MX, MY + MH, MX + MW, MY + MH);
            r.line(MX, MY, MX, MY + MH);
            r.line(MX + MW, MY, MX + MW, MY + MH);

            const std::vector<TerrainLine> &tl = terrain.getLines();
            float minTX = tl[0].x1, maxTX = tl[0].x2;
            float minTY = 9999, maxTY = 0;
            for (int i = 0; i < (int)tl.size(); i++) {
                if (tl[i].x1 < minTX) minTX = tl[i].x1;
                if (tl[i].x2 > maxTX) maxTX = tl[i].x2;
                if (tl[i].y1 < minTY) minTY = tl[i].y1;
                if (tl[i].y2 < minTY) minTY = tl[i].y2;
                if (tl[i].y1 > maxTY) maxTY = tl[i].y1;
                if (tl[i].y2 > maxTY) maxTY = tl[i].y2;
            }
            float tw = maxTX - minTX, th = maxTY - minTY;
            float ms = ((MW - 4) / tw < (MH - 4) / th) ? (MW - 4) / tw : (MH - 4) / th;
            float ox = MX + 2 + ((MW - 4) - tw * ms) / 2;
            float oy = MY + 2 + ((MH - 4) - th * ms) / 2;

            for (int i = 0; i < (int)tl.size(); i++) {
                float a = (tl[i].x1 - minTX) * ms + ox;
                float b = (tl[i].y1 - minTY) * ms + oy;
                float c = (tl[i].x2 - minTX) * ms + ox;
                float d = (tl[i].y2 - minTY) * ms + oy;
                r.line(a, b, c, d);
            }

            float smx = (ship.posX - minTX) * ms + ox;
            float smy = (ship.posY - minTY) * ms + oy;
            r.rect(smx - 1, smy - 1, 3, 3);

            for (int i = 0; i < (int)tl.size(); i++) {
                if (tl[i].labelX < 0) continue;
                float ax = (tl[i].labelX - minTX) * ms + ox;
                float ay = (tl[i].y1 - minTY) * ms + oy + 5.0f;
                if (ay + 2 > MY + MH - 1) ay = MY + MH - 3;
                if ((ship.counter / 25) & 1) continue;
                r.rect(ax, ay, 1, 1);
                r.rect(ax - 1, ay + 1, 3, 1);
            }
        }
    }

    r.flush();
}
