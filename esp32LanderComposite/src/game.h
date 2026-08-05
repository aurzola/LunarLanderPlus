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
    float powerLevel;
};

struct WindStreak {
    float x, y, vy;
};

struct DustParticle {
    float x, y, vy, life;
};

class Game {
public:
    Game();

    void newGame();
    void restartLevel();
    void nextLevel();
    void update();
    void draw(Renderer &r);

    Input input;
    int state;
    int score;
    int level;
    float fuel;
    float introTimer;
    bool demo;
    float demoTimer;

    Ship ship;
    Terrain terrain;
    float windStrength;
    int windDir;
    float landingProximity() const;

private:
    float viewX, viewY, viewScale;
    bool zoomedIn;
    float resetTimer;
    int landMultiplier;
    float demoSkill;
    float demoTargetX;
    float demoTargetY;
    float windPhase;
    float windFlipTimer;
    std::vector<WindStreak> windStreaks;
    std::vector<DustParticle> dust;
    void updateView();
    void setZoom(bool zoom);
    void checkCollisions();
    void endGame();
    void startDemo();
    void endDemoToTitle();
    void setupTitleShip();
    void runDemoAI();
    void spawnWind();
    void spawnDust();
    void updateWind(float dt);
    void drawWind(Renderer &r);
};

#endif
