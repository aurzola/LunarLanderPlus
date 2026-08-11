#ifndef RINGS_H
#define RINGS_H

#include "renderer.h"
#include "terrain.h"
#include "config.h"

// Ganymede debris band: a single thick swath of hollow rock polygons hugging
// the terrain silhouette (the same "band" concept as Titan's fog sheets, but
// made of rocks). The density is graded: small decorative rocks (no collision)
// cluster in the upper half so the approach feels light, while bigger danger
// rocks pack tighter in the lower half where the ship is more committed. A
// guaranteed passable gap (RING_GAP_MIN) is kept between danger rocks.
class Rings {
public:
    Rings();

    void reset(int level, const Terrain &t);
    bool active() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }
    void update(float dt);
    void draw(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const;

    int ringCount() const { return RING_COUNT; }
    int rocksInRing() const { return smallCount_ + dangerCount_; }
    // Band rock (rockIndex) world center in (x, y); true while the band is
    // active (rocks float above the terrain by design).
    bool rockVisible(const Terrain &t, int rockIndex, float &x, float &y) const;
    bool rockDanger(int rockIndex) const;

    // World-y of the band's center above world-x (for placing crash text
    // below it during zoom-in).
    float centerBandY(const Terrain &t, float x) const { return bandY(t, x); }

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

    int level_;
    bool enabled_;
    float t_;
    float width_;    // world width over which rocks wrap
    float cy_;       // band centre Y (world)
    float drift_;    // horizontal drift (u/s)
    int smallCount_; // deco rocks, upper half
    int dangerCount_; // danger rocks, lower half
    Rock rocks_[MAX_ROCKS];

    float rockY(const Terrain &t, int i) const;
    float bandY(const Terrain &t, float x) const;
    void tracePoly(Renderer &r, const float *px, const float *py, int n) const;
    void fillDanger(Renderer &r, const float *px, const float *py, int n) const;
    void drawFog(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const;
    static float terrainYAt(const Terrain &t, float x, float fallback);
};

#endif
