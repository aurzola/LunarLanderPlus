#ifndef GEYSERS_H
#define GEYSERS_H

#include <vector>
#include "renderer.h"
#include "terrain.h"
#include "config.h"

class Geysers {
public:
    Geysers();

    void reset(int level, const Terrain &t);
    bool active() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }
    void update(float dt);
    void draw(Renderer &r, float viewX, float viewY, float viewScale) const;
    int particlesAlive() const { return (int)parts_.size(); }
    bool inPlume(float x, float y) const;
    int ventCount() const { return (int)vents_.size(); }
    float ventX(int i) const { return vents_[i].x; }

private:
    struct Vent {
        float x, gy;
        float timer;
        float age;
        bool erupting;
    };
    struct Particle {
        float x, y, vx, vy, life, maxLife;
    };

    int level_;
    bool enabled_;
    float t_;
    std::vector<Vent> vents_;
    std::vector<Particle> parts_;

    static float terrainYAt(const Terrain &t, float x, float fallback);
    void emit(const Vent &v);
};

#endif
