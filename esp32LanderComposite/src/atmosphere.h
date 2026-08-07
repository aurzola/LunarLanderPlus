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
    void update(float dt);
    void drawSky(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const;

    // True when the ship's footprint (x, y) falls inside a fog band: the ship
    // is not drawn there, so the player flies blind until leaving the band.
    // Bands drift and undulate over time, so the blind zones can't be
    // memorized.
    bool hidesShip(float x, float y) const;
    int bandCount() const { return FOG_BAND_COUNT; }
    float bandCenter(int i) const { return bands_[i].cy; }

private:
    struct Band {
        float cy;    // world-y center of the band at t=0
        float half;  // world-y half height (mid value, undulated by waviness)
        float driftSpeed, driftPhase;
        float waveSpeed, wavePhase;
    };

    int level_;
    bool enabled_;
    float t_;
    Band bands_[FOG_BAND_COUNT];

    float centerY(int i) const;
    float halfAt(int i, float x) const;
    static float terrainYAt(const Terrain &t, float x, float fallback);
};

#endif
