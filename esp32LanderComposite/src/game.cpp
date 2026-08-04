#include <cmath>
#include <cstdio>
#include <cstring>
#include "game.h"

Game::Game()
    : state(STATE_WAITING), score(0), level(1), fuel(FUEL_MAX), introTimer(0),
      viewX(0), viewY(0), viewScale(1.0f),
      zoomedIn(false), resetTimer(0), landMultiplier(1)
{
    input.startPressed = false;
    input.angle = 0;
    input.thrust = 0;
    input.powerLevel = 0;
    terrain.init();
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
        ship.velX = 2;
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

    if (state == STATE_WAITING) {
        ship.update();
        ship.altitude = terrain.getLines()[0].y1 - ship.bottom;
        if (input.startPressed) {
            newGame();
        }
        return;
    }

    if (state == STATE_PLAYING) {
        if (introTimer > 0) {
            introTimer -= dt;
            if (introTimer < 0) introTimer = 0;
            return;
        }

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
            if (ship.fuel <= 0) {
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
        }
    }
}

void Game::draw(Renderer &r)
{
    r.clear();

    if (state == STATE_WAITING) {
        terrain.draw(r, viewX, viewY, viewScale, ship.counter);
        ship.draw(r, viewX, viewY, viewScale);
        r.text(90, 50, "LUNAR LANDER");
        r.text(72, 80, "PRESS BUTTON TO PLAY");
        r.text(90, 110, "STICK: ROTATION");
        r.text(72, 130, "POT+BTN: THRUST");
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
            const float MX = 112, MY = 22, MW = 96, MH = 54;
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
                if (ay + 3 > MY + MH - 1) ay = MY + MH - 4;
                if ((ship.counter / 25) & 1) continue;
                r.rect(ax, ay, 1, 1);
                r.rect(ax - 1, ay + 1, 3, 1);
                r.rect(ax - 2, ay + 2, 5, 1);
            }
        }
    }

    r.flush();
}
