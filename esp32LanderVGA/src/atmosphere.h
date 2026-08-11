#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H

#include "renderer.h"
#include "terrain.h"
#include "config.h"

class Atmosphere {
public:
    Atmosphere();

    void reset(int level);
    bool active() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }
    void update(float dt);
    void drawSky(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const;

    // True when the ship's footprint (x, y) falls inside a fog band: the ship
    // is not drawn there, so the player flies blind until leaving the band.
    bool hidesShip(float x, float y) const;
    int bandCount() const { return FOG_BAND_COUNT; }
    float bandCenter(int i) const { return bands_[i].cy; }

private:
    struct Band {
        float cy;    // world-y center of the band at t=0
        float half;  // world-y half height
        float driftSpeed, driftPhase;
    };

    int level_;
    bool enabled_;
    float t_;
    Band bands_[FOG_BAND_COUNT];

    float centerAt(int i, float x) const; // band center world-y (ellipse + drift)
    static float terrainYAt(const Terrain &t, float x, float fallback);
};

#endif
