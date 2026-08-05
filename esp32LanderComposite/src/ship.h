#ifndef SHIP_H
#define SHIP_H

#include <vector>

class Renderer;

struct ShipShape {
    float dx[8], dy[8];
    int count;
    bool closed;
    float velX, velY;
};

class Ship {
public:
    Ship();

    void reset(float x, float y);
    void update();
    void setTargetRotation(float deg);
    void setThrust(float power);
    void draw(Renderer &r, float viewX, float viewY, float viewScale);
    void crash();
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
    int counter;

    float left, right, bottom, top;
    float windStrength;
    int windDir;

private:
    ShipShape shapes[6];
    float shapePosX[6], shapePosY[6];
    void defineShapes();
    void updateExplosion();
};

#endif
