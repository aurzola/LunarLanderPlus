#ifndef STORM_H
#define STORM_H

#include <vector>
#include "renderer.h"
#include "terrain.h"
#include "config.h"

struct StormPoint {
    float x, y;
};

class Storm {
public:
    Storm();

    void reset(int level);
    bool active() const { return level_ >= STORM_START_LEVEL; }
    void update(float dt, const Terrain &t);
    void drawSky(Renderer &r, float viewX, float viewY, float viewScale) const;
    void drawBolts(Renderer &r, float viewX, float viewY, float viewScale) const;

    int boltsSpawned() const { return spawned_; }
    int activeBolts() const { return (int)bolts_.size(); }

private:
    struct Bolt {
        std::vector<StormPoint> path;
        float life, maxLife;
    };

    int level_;
    float nextBolt_;
    int spawned_;
    std::vector<Bolt> bolts_;

    float interval() const;
    void spawnBolt(const Terrain &t);
    static float terrainYAt(const Terrain &t, float x, float fallback);
    static void shadedLine(Renderer &r, float x0, float y0, float x1, float y1, int brightness);
    static void drawPolyline(Renderer &r, const std::vector<StormPoint> &pts,
                             float viewX, float viewY, float viewScale, int brightness);
    void drawBolt(Renderer &r, const Bolt &b, float viewX, float viewY,
                  float viewScale) const;
};

#endif
