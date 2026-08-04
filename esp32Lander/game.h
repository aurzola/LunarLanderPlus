#ifndef GAME_H
#define GAME_H

#include <vector>
#include "ship.h"
#include "terrain.h"
#include "renderer.h"
#include "config.h"

enum GameState {
    STATE_WAITING = 0,
    STATE_PLAYING = 1,
    STATE_LANDED = 2,
    STATE_CRASHED = 3,
    STATE_GAMEOVER = 4
};

struct Input {
    bool startPressed;
    float angle;
    float thrust;
};

class Game {
public:
    Game();

    void newGame();
    void restartLevel();
    void update();
    void draw(Renderer &r);

    Input input;
    int state;
    int score;
    float fuel;

    Ship ship;
    Terrain terrain;

private:
    float viewX, viewY, viewScale;
    bool zoomedIn;
    float resetTimer;
    int landMultiplier;
    void updateView();
    void setZoom(bool zoom);
    void checkCollisions();
};

#endif
