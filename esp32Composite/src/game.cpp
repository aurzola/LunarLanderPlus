#include <cmath>
#include <cstdio>
#include "game.h"

Game::Game()
{
    state = STATE_MENU;
    score = 0;
    multiplier = 1;
    gas = 0;
    playing = true;
    collided = 0;
    landingType = 0;
    resetScheduled = false;
    resetTimer = 0.0f;
    input.startPressed = false;
    input.angle = 0.0f;
    input.throttle = 0;
}

void Game::resetGame()
{
    state = STATE_PLAYING;
    score = 0;
    ship.setGas(750);
    resetLife();
}

void Game::resetLife()
{
    ship.setPos(100, 100);
    ship.setVel(50, 0);
    ship.setAng(0);
    ship.setAccMode(0);
    terrain.generate((int)WORLD_W, (int)WORLD_H, 450.0f, 1);
    collided = 0;
    multiplier = 1;
    xTerrain = terrain.getXPoints();
    yTerrain = terrain.getYPoints();
    playing = true;
    resetScheduled = false;
    resetTimer = 0.0f;
    landingType = 0;
}

void Game::update()
{
    double dt = GAME_DT;

    if (state == STATE_MENU) {
        if (input.startPressed) resetGame();
    } else if (state == STATE_PLAYING) {
        if (playing) {
            gas = ship.getGas();
            xVel = ship.getXvel();
            yVel = ship.getYvel();
            x = ship.getXpos();
            y = ship.getYpos();
            ang = ship.getAng();

            y = y + yVel * dt + .5 * 30. * dt * dt;
            x = x + xVel * dt;

            if (x > WORLD_W + 10) x = -10;
            else if (x < -10) x = WORLD_W + 10;

            if (y < -50) {
                x = 100;
                y = 100;
                xVel = 50;
                yVel = 0;
                ship.setAng(0);
                ship.setAccMode(0);
            }

            if (xVel >= 100) xVel = 100;
            else if (xVel <= -100) xVel = -100;

            yVel = yVel + 10. * dt;

            ship.accelerate(xVel, yVel);
            ship.setPos(x, y);
            ship.setVel(xVel, yVel);

            if (input.angle > 0.0f) ship.setAng(0.0);
            else if (input.angle < -PI) ship.setAng(-PI);
            else ship.setAng((double)input.angle);

            if (input.throttle > 8) ship.setAccMode(8);
            else if (input.throttle < 0) ship.setAccMode(0);
            else ship.setAccMode(input.throttle);

            if (ship.getGas() <= 0) ship.setAccMode(0);

            ship.hitbox();
            collided = ship.collision(xTerrain, yTerrain);

            if (collided == 1) {
                playing = false;
                resetScheduled = true;
                resetTimer = 4.0f;
                score += 5;
                ship.setGas(ship.getGas() - 100);
                gas = ship.getGas();
                landingType = 0;
            } else if (collided == 2) {
                playing = false;
                resetScheduled = true;
                resetTimer = 4.0f;
                multiplier = terrain.multiplierCheck((int)ship.getXpos());
                if (yVel < 12 && fabs(xVel) < 25) {
                    landingType = 1;
                    ship.setGas(ship.getGas() + 50);
                    score += multiplier * 50;
                } else if (yVel < 25 && fabs(xVel) < 25) {
                    landingType = 2;
                    score += multiplier * 15;
                } else {
                    landingType = 3;
                    score += 5;
                    ship.setGas(ship.getGas() - 100);
                }
                gas = ship.getGas();
            }

            if (ship.getGas() <= 0) {
                state = STATE_GAMEOVER;
                playing = false;
                resetScheduled = true;
                resetTimer = 5.0f;
            }
        } else {
            resetTimer -= dt;
            if (resetTimer <= 0.0f && resetScheduled) resetLife();
        }
    } else if (state == STATE_GAMEOVER) {
        resetTimer -= dt;
        if (resetTimer <= 0.0f) {
            state = STATE_MENU;
            resetScheduled = false;
        }
    }
}

void Game::draw(Renderer &r)
{
    r.clear();
    char buf[80];

    if (state == STATE_MENU) {
        r.text(116, 60, "LUNAR LANDER");
        r.text(124, 82, "PRESS P TO PLAY");
        r.text(116, 120, "CONTROLS:");
        r.text(108, 134, "POT: ANGLE");
        r.text(100, 146, "TRIGGER: THRUST");
        r.text(100, 158, "BUTTON: START");
    } else if (state == STATE_PLAYING || state == STATE_GAMEOVER) {
        terrain.draw(r);
        ship.draw(r);

        snprintf(buf, sizeof buf, "SCORE %d", score); r.text(6, 6, buf);
        snprintf(buf, sizeof buf, "FUEL %d", (int)gas); r.text(6, 16, buf);
        snprintf(buf, sizeof buf, "ALT %d", (int)(WORLD_H - 10 - y)); r.text(232, 6, buf);
        snprintf(buf, sizeof buf, "VX %d", (int)xVel); r.text(232, 16, buf);
        snprintf(buf, sizeof buf, "VY %d", (int)(-yVel)); r.text(232, 26, buf);

        if (state == STATE_PLAYING && collided != 0) {
            if (collided == 1) {
                r.text(112, 108, "YOU CRASHED");
                r.text(86, 120, "LOST 100 FUEL UNITS");
            } else if (collided == 2) {
                if (landingType == 1) {
                    r.text(110, 108, "GOOD LANDING");
                    r.text(84, 120, "50 FUEL UNITS ADDED");
                } else if (landingType == 2) {
                    r.text(110, 108, "HARD LANDING");
                } else {
                    r.text(112, 108, "YOU CRASHED");
                    r.text(86, 120, "LOST 100 FUEL UNITS");
                }
            }
        } else if (state == STATE_GAMEOVER) {
            r.text(84, 108, "YOU RAN OUT OF FUEL");
            r.text(130, 120, "GAME OVER");
        }
    }
    r.flush();
}
