#ifndef TWISTER_H
#define TWISTER_H

#include "renderer.h"
#include "terrain.h"
#include "config.h"
#include "ship.h"

// Triton nitrogen twister: a wandering vortex that sucks the ship toward its
// ground base. `apply()` drives the physics directly on the Ship (radial pull,
// tangential swirl and downward sink). Escape is physical: when the ship's
// outward radial thrust overcomes the strength-scaled pull and it already
// moves outward, it is violently flung along the tangent (the tangential
// velocity it built up while orbiting) and the nose gets a yank, so the player
// must recover heading. `captured()` is true while the vortex is dragging the
// ship, so Game can treat any ground contact as a twister crash.
class Twister {
public:
    Twister();

    void reset(int level, const Terrain &t);
    bool active() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }
    void update(float dt);
    bool apply(Ship &s, const Terrain &t, float stickDeg = 0.0f);
    bool captured() const { return captured_; }
    bool justEscaped() const { return escaped_; }

    // For tests / demo.
    float coreX() const { return cx_; }
    float coreY(const Terrain &t) const { return terrainYAt(t, cx_, 480.0f); }
    float strength() const { return strength_; }

    void draw(Renderer &r, const Terrain &t,
              float viewX, float viewY, float viewScale,
              bool zoomedIn) const;

private:
    int level_;
    bool enabled_;
    float t_;
    float cx_;
    float drift_;
    float strength_;
    int swirl_;
    float phase_;
    float escapeCooldown_;
    float holdT_;
    int escapeTicks_;
    float capOff_;
    float swirlAngle_;
    float weavePrevX_;
    bool captured_;
    bool escaped_;

    static float terrainYAt(const Terrain &t, float x, float fallback);
};

#endif
