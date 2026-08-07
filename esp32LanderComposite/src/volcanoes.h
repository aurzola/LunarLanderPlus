#ifndef VOLCANOES_H
#define VOLCANOES_H

#include <vector>
#include "renderer.h"
#include "terrain.h"
#include "config.h"

struct LavaRange {
    float x1, x2;
};

class Volcanoes {
public:
    Volcanoes();

    void reset(int level, const Terrain &t);
    bool active() const { return enabled_; }
    void update(float dt);
    void draw(Renderer &r, float viewX, float viewY, float viewScale) const;
    int particlesAlive() const { return (int)parts_.size(); }
    int volcanoCount() const { return (int)volc_.size(); }

    // True if the ship footprint [left,right] overlaps lava poured onto a
    // landing pad. Only the near portion of a pad can be covered (capped), so
    // the far side stays clear for a safe landing.
    bool landOnLava(float left, float right) const;
    int lavaRangeCount() const { return (int)lava_.size(); }
    float lavaRangeX1(int i) const { return lava_[i].x1; }
    float lavaRangeX2(int i) const { return lava_[i].x2; }

private:
    struct Volcano {
        float x, gy;
        float timer, age;
        float phase;
        bool erupting;
        std::vector<float> flowX, flowY;
        std::vector<float> flowX2, flowY2;
    };
    struct Particle {
        float x, y, vx, vy, life, maxLife;
    };

    int level_;
    bool enabled_;
    float t_;
    std::vector<Volcano> volc_;
    std::vector<Particle> parts_;
    std::vector<LavaRange> lava_;

    static float terrainYAt(const Terrain &t, float x, float fallback);
    void buildFlow(const Terrain &t, Volcano &v, float w);
    void emit(const Volcano &v);
    void computeLava(const Terrain &t);
    void clampFlows();
};

#endif
