#ifndef QUAKE_H
#define QUAKE_H

#include <vector>
#include "renderer.h"
#include "terrain.h"
#include "ship.h"

class Quake {
public:
    enum Phase { IDLE, RUMBLING, BROKEN };

    struct Dust {
        float x, y, vx, vy, life, maxLife;
    };

    Quake();

    void reset(int level, const Terrain &t, const Ship &s);
    bool active() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }
    Phase phase() const { return phase_; }
    bool justStruck() const { return justStruck_; }
    bool justRumbled() const { return justRumbled_; }
    float shake() const { return shake_; }
    int rupturedZone() const { return rupturedZone_; }
    float strikeX() const { return targetX_; }
    bool isRupturedAt(float x) const {
        return x >= rupturedX1_ && x <= rupturedX2_;
    }
    void update(float dt, Terrain &t, const Ship &s, bool canStrike = true);
    void draw(Renderer &r, float viewX, float viewY, float viewScale);

private:

    bool enabled_;
    Phase phase_;
    float timer_;
    float rumbleT_;
    float rearmT_;
    int rupturedZone_;
    float targetX_;
    float targetGroundY_;
    float rupturedX1_, rupturedX2_;
    float shake_;
    bool justStruck_;
    bool justRumbled_;
    std::vector<Dust> dust_;
};

#endif
