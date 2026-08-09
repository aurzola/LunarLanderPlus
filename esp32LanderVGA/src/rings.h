#ifndef RINGS_H
#define RINGS_H

#include "renderer.h"
#include "terrain.h"
#include "config.h"

// Ganymede debris bands (franjas de roca): the moon's debris is rendered as a
// few bands of hollow rock polygons hugging the terrain silhouette (the same
// "band" concept as Titan's fog sheets, but made of rocks). Each band sits at
// a fixed height above the terrain and is filled with danger rocks drawn only
// as outlines (hollow), of varying irregular shape and size. The ship must
// weave down through both bands without touching a rock. The higher band is
// denser (harder), the lower sparser, but both keep a guaranteed passable gap.
class Rings {
public:
    Rings();

    void reset(int level, const Terrain &t);
    bool active() const { return enabled_; }
    void update(float dt);
    void draw(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const;

    int ringCount() const { return RING_COUNT; }
    int rocksInRing(int ringIndex) const;
    // Band rock (ringIndex, rockIndex) world center in (x, y); true while the
    // band is active (rocks float above the terrain by design).
    bool rockVisible(const Terrain &t, int ringIndex, int rockIndex, float &x, float &y) const;
    bool rockDanger(int ringIndex, int rockIndex) const;

    // World-y of the lower band's center above world-x (for placing crash text
    // below it during zoom-in).
    float lowerBandY(const Terrain &t, float x) const { return bandY(t, 1, x); }

    // True if any danger rock intersects the ship circle (sx, sy, shipR).
    bool hitsShip(const Terrain &t, float sx, float sy, float shipR) const;

private:
    enum { MAX_VERTS = 8, MAX_ROCKS = 60 };

    struct Rock {
        float x;      // world x (drifts and wraps over the band)
        float size;   // nominal world radius (collision = this value)
        float yOff;   // fixed vertical scatter within the band
        float rot;    // current polygon rotation
        float spin;   // rotation speed
        int nVerts;   // polygon vertex count
        float vrad[MAX_VERTS]; // per-vertex radius factor
    };

    struct Band {
        float cy;     // world-y base center of the ring
        float drift;  // horizontal drift speed (u/s)
        int smallCount; // small decorative rocks (no collision)
        int dangerCount; // big dangerous rocks (collide)
        Rock rocks[MAX_ROCKS];
    };

    int level_;
    bool enabled_;
    float t_;
    float width_;    // world width over which rocks wrap
    Band bands_[RING_COUNT];
    mutable int frameCtr_;

    float rockY(const Terrain &t, int b, int i) const;
    float bandY(const Terrain &t, int b, float x) const;
    float bandCy(int b) const { return bands_[b].cy; }
    void tracePoly(Renderer &r, const float *px, const float *py, int n) const;
    void fillDanger(Renderer &r, const float *px, const float *py, int n) const;
    void drawFog(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const;
    static float terrainYAt(const Terrain &t, float x, float fallback);
};

#endif
