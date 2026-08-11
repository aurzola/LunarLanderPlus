#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "wormhole.h"
#include "terrain.h"
#include "ship.h"
#include "renderer_pc.h"
#include "config.h"

// Visual demo of the sky wormhole: the spiral accretion disk fades in, the
// PURE RADIAL pull accelerates the ship toward the nucleus while it is still
// in the outer zone (d >= CAPTURE_R), and once the ship crosses half the
// action radius it is CAPTURED: it can no longer escape and the hole spirals
// it in a vortex (shrinking, nose to the core) until it is swallowed at the
// rim (bright ring flash) and the disk fades out. Renders PPM frames to
// frames/. Selftest: the phase machine runs EMERGING -> ACTIVE -> SWALLOW ->
// DYING and the ship ends swallowed.
int main(int argc, char **argv)
{
    int seed = (argc > 1) ? atoi(argv[1]) : 1;
    int level = (argc > 2) ? atoi(argv[2]) : 1;
    srand(1000 + seed);

    RendererPC r((int)SCREEN_W, (int)SCREEN_H, "frames");
    Terrain t;
    Wormhole w;
    Ship s;

    t.generate(level);

    // The hole floats in the sky, clear of the terrain profile.
    float cx = 260.0f + (float)(rand() % 300);
    float cy = 150.0f + (float)(rand() % 50);
    w.reset(cx, cy);

    // Ship starts below-left of the hole, in the OUTER zone: it first feels
    // the radial pull, then crosses the capture radius and is vortexed in.
    s.reset(cx + 110.0f, cy + 40.0f);
    s.velX = 0.0f;
    s.velY = 0.0f;
    s.scale = 1.0f;

    const float viewScale = SCREEN_H / 700.0f;
    const float viewX = 0.0f, viewY = 0.0f;

    bool sawEmerging = false;
    bool sawActive = false;
    bool sawCaptured = false;
    int frames = (int)((WORMHOLE_EMERGE_T + 6.0f + 0.15f +
                        WORMHOLE_DIE_T) / GAME_DT) + 60;
    for (int i = 0; i < frames; i++) {
        w.update(GAME_DT);
        if (w.active() && !w.swallowed()) w.apply(s);
        if (!w.swallowed()) {
            s.posX += s.velX;
            s.posY += s.velY;
            s.velX *= DRAG; // same damping as Ship::update
            s.velY *= DRAG;
            // Nose turns toward the nucleus during the vortex (Ship::update
            // would lerp this in the real game; simulate it here).
            s.rotation += (s.targetRotation - s.rotation) * ROTATION_LERP;
            if (fabsf(s.rotation - s.targetRotation) < 0.1f)
                s.rotation = s.targetRotation;
        }

        if (w.phase() == WH_EMERGING) sawEmerging = true;
        if (w.phase() == WH_ACTIVE) sawActive = true;
        if (w.captured()) sawCaptured = true;

        r.clear();
        t.draw(r, viewX, viewY, viewScale, 0);
        w.draw(r, viewX, viewY, viewScale);
        if (!w.swallowed()) s.draw(r, viewX, viewY, viewScale);
        if (i % 5 == 0 || i == frames - 1) r.flush();
    }

    printf("wormhole level=%d core=(%.0f,%.0f) phases: emerging=%d active=%d "
           "captured=%d swallowed=%d finalPhase=%d t=%.0fs\n",
           level, cx, cy, sawEmerging ? 1 : 0, sawActive ? 1 : 0,
           sawCaptured ? 1 : 0, w.swallowed() ? 1 : 0, (int)w.phase(),
           frames * GAME_DT);
    if (!sawEmerging || !sawActive || !sawCaptured || !w.swallowed()) {
        fprintf(stderr, "FAIL: phase sequence incomplete\n");
        return 1;
    }
    printf("OK: %d frames rendered to frames/\n", frames / 5);
    return 0;
}
