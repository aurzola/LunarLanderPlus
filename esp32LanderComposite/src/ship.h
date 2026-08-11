#ifndef SHIP_H
#define SHIP_H

#include <vector>
#include "config.h"

class Renderer;

struct ShipShape {
    float dx[8], dy[8];
    int count;
    bool closed;
    float velX, velY;
};

struct GroundParticle {
    float x, y;
    float velX, velY;
    float life;
    float size;   // 1 = dot, 2 = plus, 3 = 3x3 rock chunk
    float shade;  // per-particle brightness multiplier (0.7..1.0)
    bool active;
};

class Ship {
public:
    Ship();

    void reset(float x, float y);
    void update();
    void setTargetRotation(float deg);
    void setThrust(float power);
    void draw(Renderer &r, float viewX, float viewY, float viewScale, float melt = 0.0f);
    void crash(bool fuel = false);
    void dissolve();
    void land();

    float posX, posY;
    float velX, velY;
    float rotation;
    float targetRotation;
    float thrustBuild;
    float fuel;
    float scale;
    float altitude;
    bool active;
    bool exploding;
    bool dissolving;
    int dissolveTick;
    bool fuelExplosion;
    int counter;

    float left, right, bottom, top;
    float windStrength;
    int windDir;
    float gravity;
    bool chute;          // parachute deployed (one-shot per level)
    float chuteOpen;     // 0..1 canopy opening ramp (physics + visual)

private:
    ShipShape shapes[6];
    float shapePosX[6], shapePosY[6];
    GroundParticle groundParticles[GROUND_PARTICLES_MAX];
    void defineShapes();
    void initGroundParticles();
    void updateExplosion();
};

#endif
