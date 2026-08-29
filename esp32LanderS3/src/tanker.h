#ifndef TANKER_H
#define TANKER_H

#include "renderer.h"
#include "terrain.h"
#include "ship.h"
#include "config.h"

class Tanker {
public:
    Tanker();

    void reset(int level, const Terrain& terrain, float fuel, bool force = false);
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
    // Re-dock cooldown after breaking away: the probe cannot re-seat while
    // this is > 0, so a freshly ejected module does not snap straight back in.
    float redockCooldown = 0.0f;

    // Drogue basket visual. x,y are screen coordinates of the basket balance
    // point (the physics align point), s the px-per-world-unit scale.
    // drawDrogue = small guide triangle for the main view; drawDroguePip = the
    // magnified receiving basket (inverted truncated cone) inside the PiP.
    static void drawDrogue(Renderer &r, float x, float y, float s);
    static void drawDroguePip(Renderer &r, float x, float y, float s);

    // Drogue basket at the end of the refueling hose (probe-and-drogue).
    float drogueX() const;
    float drogueY() const;

    // Effective draw scale (px per world unit) applied to both the tanker and
    // the ship in their draw(): the same factor (ship.scale * viewScale). The
    // tanker reads bigger than the ship purely from its larger base geometry
    // (scaled by TANKER_SIZE), not from a draw-scale boost.
    static float drawScaleFor(float shipScale, float viewScale) {
        return shipScale * viewScale;
    }

    // Intrinsic stay-size multiplier for the whole tanker (draw + physics).
    static float size() { return TANKER_SIZE; }

    // Scaled accessors for the tanker's physical / interface dimensions. Each
    // derives from a base constant times TANKER_SIZE, so resizing the tanker
    // (TANKER_SIZE) scales rendering, hitbox, portY, hose, docking tolerances
    // and the trigger zone together.
    float hullHalfW() const { return (TANKER_HULL_W * 0.5f + TANKER_HULL_MARGIN) * TANKER_SIZE; }
    float balloonHalfH() const { return (5.5f + TANKER_HULL_MARGIN * 0.5f) * TANKER_SIZE; }
    float hullHalfH() const { return TANKER_HULL_H * 0.5f * TANKER_SIZE; }
    float hoseLen() const { return TANKER_HOSE_LEN * TANKER_SIZE; }
    float dockTolX() const { return TANKER_DOCK_TOL_X * TANKER_SIZE; }
    float dockTolY() const { return TANKER_DOCK_TOL_Y * TANKER_SIZE; }
    float dockBreakTolX() const { return TANKER_DOCK_BREAK_TOL_X * TANKER_SIZE; }
    float dockBreakTolY() const { return TANKER_DOCK_BREAK_TOL_Y * TANKER_SIZE; }
    float dockZoneX() const { return TANKER_DOCK_ZONE_X * TANKER_SIZE; }
    float dockZoneY() const { return TANKER_DOCK_ZONE_Y * TANKER_SIZE; }
    float platformW() const { return TANKER_PLATFORM_W * TANKER_SIZE; }

private:
    float driftX;
    float phase;
    float droguePhase;
    void move(float dt);
};

#endif
