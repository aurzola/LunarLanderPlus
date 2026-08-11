#ifndef WORMHOLE_H
#define WORMHOLE_H

#include "config.h"

class Renderer;
class Ship;

// Wormhole sky effect: a spiral accretion disk (drawn slightly squashed so it
// reads as a tilted disc) with a dark nucleus and a bright photon ring. It
// fades in at a random sky position (EMERGING) and then acts in two zones:
// OUTSIDE half the action radius (d >= WORMHOLE_CAPTURE_R) it exerts a PURE
// RADIAL pull (a = WORMHOLE_PULL_MAX*(1-d/GRAB_R)) the ship can escape by
// thrusting away from the center; INSIDE that radius the ship is captured and
// can no longer escape -- the hole spirals it in a VORTEX toward the nucleus
// (shrinking, nose to the core) until it crosses the rim (d < SWALLOW_R) and
// is swallowed (WH_SWALLOW flash, then WH_DYING). Game hookup: while swallowed
// Game hides the ship, waits for the hole to fade and then fades the ship in
// on another moon between the sky and the terrain (wormholeJump). The hole
// stays active for the whole level and never coexists with any other effect.
enum WormholePhase {
    WH_IDLE = 0,
    WH_EMERGING,
    WH_ACTIVE,
    WH_SWALLOW,
    WH_DYING,
};

class Wormhole {
public:
    Wormhole();

    void reset(float cx, float cy);
    void update(float dt);
    bool apply(Ship &s);      // radial pull (outer zone) or scripted vortex
                              // (captured zone); true when the ship's position
                              // was scripted/this frame it was swallowed
    void draw(Renderer &r, float viewX, float viewY, float viewScale) const;
    void disable() { enabled_ = false; phase_ = WH_IDLE; swallowed_ = false; captured_ = false; }

    bool active() const { return enabled_; }
    bool captured() const { return captured_; } // inside the vortex, no escape
    WormholePhase phase() const { return phase_; }
    bool swallowed() const { return swallowed_; }
    float coreX() const { return cx_; }
    float coreY() const { return cy_; }

private:
    bool enabled_;
    WormholePhase phase_;
    float t_;
    float cx_, cy_;
    int spin_;
    float rot_;
    float phaseSeed_;
    bool swallowed_;
    bool captured_;
    float captureT_;     // s elapsed in the vortex
    float startAng_;     // polar angle of the ship around the core at capture
    float captureRad_;   // squashed-frame distance to the core at capture
                         // (captureRad_/startAng_ in SQUASH-scaled coords make
                         // the spiral start exactly at the ship's position)
    float captureScale_; // ship.scale at capture (shrink base)
};

#endif
