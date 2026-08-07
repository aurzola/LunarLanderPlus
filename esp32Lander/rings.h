#ifndef RINGS_H
#define RINGS_H

#include "renderer.h"
#include "terrain.h"
#include "config.h"

// Debris rings (Ganymede): two concentric rings of orbiting rock. The inner
// ring turns slowly, the outer one faster and in the opposite direction, so
// the gaps between rocks are never static. Each ring is a circle centered at
// (RING_CX, RING_CY); a rock only exists (and collides) when it is above the
// terrain silhouette at its x - the far side of the ring is hidden behind the
// moon itself. The objective is to weave the descending ship through without
// hitting a rock.
class Rings {
public:
    Rings();

    void reset(int level);
    bool active() const { return enabled_; }
    void update(float dt);
    void draw(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const;

    int ringCount() const { return RING_COUNT; }
    int rocksInRing(int ringIndex) const;
    // Rock (ringIndex, rockIndex) world center in (x, y); false if that rock
    // is at/below the terrain surface there (hidden behind the moon), in which
    // case it neither draws nor collides.
    bool rockVisible(const Terrain &t, int ringIndex, int rockIndex, float &x, float &y) const;

    // True if any visible ring rock intersects the ship circle (sx, sy, shipR).
    bool hitsShip(const Terrain &t, float sx, float sy, float shipR) const;

private:
    struct Ring {
        float radius;
        float w;       // angular speed (rad/s; negative = reverse direction)
        float phase;
    };

    int level_;
    bool enabled_;
    float t_;
    Ring rings_[RING_COUNT];

    static float terrainYAt(const Terrain &t, float x, float fallback);
    void rockPos(int ringIndex, int rockIndex, float &x, float &y) const;
};

#endif
