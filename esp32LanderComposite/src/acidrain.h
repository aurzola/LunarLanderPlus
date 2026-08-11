#ifndef ACIDRAIN_H
#define ACIDRAIN_H

#include <vector>
#include "renderer.h"
#include "terrain.h"
#include "config.h"

// Acid rain on Europa: a few drifting storm cells. Rain inside a cell
// corrodes the ship (ACID meter 0-100%); at 100% the ship is destroyed.
// The meter dries off slowly outside the rain.
class AcidRain {
public:
    AcidRain();

    void reset(int level, const Terrain &t);
    bool active() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }
    void update(float dt);
    void draw(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const;
    void drawSizzle(Renderer &r, float shipX, float shipY, float viewX, float viewY,
                    float viewScale) const;

    bool inRain(float x, float y) const;
    void corrode();               // Game: while the ship is inside the rain
    void dry();                   // Game: while the ship is outside the rain
    float meterGet() const { return meter_; }
    int cellCount() const { return (int)cells_.size(); }
    float cellX(int i) const { return cells_[i].x; }
    void setMeter(float v) { if (v < 0) v = 0; if (v > 100) v = 100; meter_ = v; }

private:
    struct Cell {
        float x;  // world-x center
        float dir; // +1 / -1 drift direction
    };

    int level_;
    bool enabled_;
    float t_;
    float meter_;
    float worldW_;
    std::vector<Cell> cells_;
};

#endif
