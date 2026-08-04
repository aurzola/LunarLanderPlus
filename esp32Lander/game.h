#ifndef GAME_H
#define GAME_H

#include <vector>
#include "ship.h"
#include "terrain.h"
#include "renderer.h"
#include "config.h"

enum GameState {
    STATE_MENU = 1,
    STATE_PLAYING = 2,
    STATE_GAMEOVER = 3
};

struct Input {
    bool startPressed;
    float angle;
    int throttle;
};

class Game {
public:
    Game();

    void resetGame();
    void resetLife();
    void update();
    void draw(Renderer &r);

    Input input;
    int state;
    int score;
    int multiplier;
    double gas;
    bool playing;
    int collided;
    int landingType;

    double xVel, yVel, x, y, ang;

    Ship ship;
    Terrain terrain;

private:
    std::vector<int> xTerrain;
    std::vector<int> yTerrain;
    bool resetScheduled;
    float resetTimer;
};

#endif
