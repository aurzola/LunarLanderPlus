#ifndef GAME_H
#define GAME_H

#include <vector>
#include "ship.h"
#include "terrain.h"
#include "renderer.h"
#include "storm.h"
#include "geysers.h"
#include "volcanoes.h"
#include "atmosphere.h"
#include "rings.h"
#include "twister.h"
#include "tanker.h"
#include "explosion.h"
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
    bool chuteToggle;  // rising edge of C+Z together: deploy the parachute
};

struct WindStreak {
    float x, y, vy;
    float f1, f2;
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
    Storm storm;
    Geysers geysers;
    Volcanoes volcanoes;
    Atmosphere atmosphere;
    Rings rings;
    Twister twister;
    Tanker tanker;
    ExplosionManager explosion;
    bool windEnabled;
    float windStrength;
    int windDir;
    float landingProximity() const;
    bool lavaBurnGet() const { return lavaBurn; }
    bool ringHitGet() const { return ringHit; }
    bool twisterCrashGet() const { return twisterCrash; }
    bool tankerCrashGet() const { return tankerCrash; }
    float chuteTooLow() const { return chuteTooLowTimer; }
    bool chuteAvailable;

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
    float stormHitTimer;
    float fuelMaxTimer;
    float chuteTooLowTimer;
    bool demoHoldAltitude;
    bool lavaBurn;
    bool ringHit;
    bool twisterCrash;
    bool tankerCrash;
    bool explosionInited;
    int demoTankerPhase; // 0 = approach pre-position left of the drogue, 1 = slide in
    std::vector<WindStreak> windStreaks;
    std::vector<DustParticle> dust;
    void updateView();
    void setZoom(bool zoom, float zm = 5.0f);
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
    void drawDockingPiP(Renderer &r);
};

#endif
