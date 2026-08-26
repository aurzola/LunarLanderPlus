#ifndef TANKER_H
#define TANKER_H

#include "renderer.h"
#include "terrain.h"
#include "ship.h"
#include "config.h"

class Tanker {
public:
    Tanker();

    void reset(int level, const Terrain& terrain, float fuel);
    void setEnabled(bool on) { active = on; } // wormhole isolation: no tanker on wormhole moons
    bool checkDock(const Ship& ship);
    void beginDock(float shipVX, float shipVY);
    void breakAway(Ship& ship);
    bool update(float dt, Ship& ship);
    bool targeted() const;
    bool hitsHull(float shipX, float shipY) const;
    void destroy();
    void draw(Renderer &r, float viewX, float viewY, float viewScale,
              int counter, const Ship& ship) const;

    bool active;
    bool docked;
    bool leaving;
    bool done;
    float bodyX;
    float bodyY;
    float baseY;
    float originalBodyX;
    float portY;

    // Docking mini-game state: the joystick controls a small relative offset
    // from the drogue. Hold the probe within LOCK tolerance for LOCK_TIME to
    // start fuel flow; drift outside BREAK tolerance for BREAK_TIME to break.
    float dockOffsetX = 0.0f;
    float dockOffsetY = 0.0f;
    float dockLockTimer = 0.0f;
    float dockBreakTimer = 0.0f;
    bool fuelFlowing = false;

    // Drogue basket visual. x,y are screen coordinates of the basket balance
    // point (the physics align point), s the px-per-world-unit scale.
    // drawDrogue = small guide triangle for the main view; drawDroguePip = the
    // magnified receiving basket (inverted truncated cone) inside the PiP.
    static void drawDrogue(Renderer &r, float x, float y, float s);
    static void drawDroguePip(Renderer &r, float x, float y, float s);

    // Drogue basket at the end of the refueling hose (probe-and-drogue).
    float drogueX() const;
    float drogueY() const;

    // Effective draw scale (px per world unit) applied to the whole tanker in
    // draw(): the same base factor as the ship (ship.scale * viewScale) plus a
    // constant boost (TANKER_DRAW_SCALE) to keep the tanker visually bigger.
    static float drawScaleFor(float shipScale, float viewScale) {
        return shipScale * viewScale * TANKER_DRAW_SCALE;
    }

private:
    float driftX;
    float phase;
    float droguePhase;
    void move(float dt);
};

#endif
