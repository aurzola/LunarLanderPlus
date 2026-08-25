#ifndef GAME_H
#define GAME_H

#include <vector>
#include "ship.h"
#include "terrain.h"
#include "renderer.h"
#include "bglayer.h"
#include "storm.h"
#include "geysers.h"
#include "volcanoes.h"
#include "atmosphere.h"
#include "acidrain.h"
#include "rings.h"
#include "twister.h"
#include "tanker.h"
#include "wormhole.h"
#include "quake.h"
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
    Wormhole wormhole;
    AcidRain acidrain;
    Quake quake;
    ExplosionManager explosion;
    bool windEnabled;
    float windStrength;
    int windDir;
    float landingProximity() const;
    bool lavaBurnGet() const { return lavaBurn; }
    bool ringHitGet() const { return ringHit; }
    bool twisterCrashGet() const { return twisterCrash; }
    bool tankerCrashGet() const { return tankerCrash; }
    bool acidBurnGet() const { return acidBurn; }
    bool quakeCrashGet() const { return quakeCrash; }
    bool landPerfectGet() const { return landPerfect; }
    float chuteTooLow() const { return chuteTooLowTimer; }
    float warpIn() const { return warpInT; }
    float recycledBanner() const { return recycledTimer; }
    bool bgActive() const { return worldBg.ready() && bgBaked; }
    bool chuteAvailable;
    float hullIntegrity;

private:
    float viewX, viewY, viewScale;
    bool zoomedIn;
    float resetTimer;
    int landMultiplier;
    bool landPerfect;
    int landFuelBonus;
    float demoSkill;
    float demoTargetX;
    float demoTargetY;
    float windPhase;
    float windFlipTimer;
    float stormHitTimer;
    float fuelMaxTimer;
    float chuteTooLowTimer;
    float warpInT; // wormhole respawn: ship materializes (scale 0->1.5) over this
    float recycledTimer; // wormhole respawn: shows the "recycled" banner while > 0
    bool demoHoldAltitude;
    bool demoFirstLevelPending; // TEMP: demo always opens on DEMO_LEVEL_FIRST (unused; revert)
    bool lavaBurn;
    bool ringHit;
    bool twisterCrash;
    bool tankerCrash;
    bool acidBurn;
    bool quakeCrash;
    bool explosionInited;
    int demoTankerPhase; // 0 = approach pre-position left of the drogue, 1 = slide in
    std::vector<WindStreak> windStreaks;
    std::vector<DustParticle> dust;
    void updateView();
    void setZoom(bool zoom, float zm = 5.0f);
    // Viewport culling: an effect is only updated/drawn while it can be seen
    // (or, for the physics hooks, while it can reach the ship). World-space
    // point/vertical-band vs the visible world rect from the current view.
    bool effectVisible(float wx, float wy, float margin) const;
    bool xInView(float wx, float margin) const;
    bool bandVisible(float wy, float margin) const;
    bool atmosphereInView() const;
    void checkCollisions();
    void endGame();
    void startDemo();
    void endDemoToTitle();
    void setupTitleShip();
    void runDemoAI();
    void setupDemoTarget();
    void spawnWormhole(bool force = false);
    void wormholeJump();
    void isolateForWormhole();
    void spawnWind();
    void spawnDust();
    void updateWind(float dt);
    void drawWind(Renderer &r);
    void drawDockingPiP(Renderer &r);
    void bakeBg();

    BgLayer worldBg;
    unsigned bgBakedRev;
    bool bgBaked;
    bool bgAllocFailed;
};

#endif
