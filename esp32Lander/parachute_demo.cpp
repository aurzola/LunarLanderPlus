#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "ship.h"
#include "terrain.h"
#include "renderer_pc.h"
#include "config.h"

// Visual check of the parachute canopy: renders the ship with the canopy at
// progressive inflation stages (packed, 25%, 50%, 75%, fully open) plus the
// deployment ramp over a few seconds of physics. PPM snapshots to frames/.
int main(int argc, char **argv)
{
    int seed = (argc > 1) ? atoi(argv[1]) : 1;
    srand(1000 + seed);

    RendererPC r((int)SCREEN_W, (int)SCREEN_H, "frames");
    Terrain t;
    Ship ship;

    t.generate(2);
    ship.reset(400.0f, 250.0f);
    ship.scale = 1.0f;

    const float viewScale = SCREEN_H / 700.0f;
    const float viewX = 0.0f, viewY = 0.0f;

    // Static inflation stages, drawn 50 frames apart so the snapshots land on
    // the flush ticks (every 5 frames).
    for (int stage = 0; stage < 5; stage++) {
        for (int f = 0; f < 50; f++) {
            ship.chute = true;
            ship.chuteOpen = stage / 4.0f;
            ship.velY = 0.09f;
            r.clear();
            t.draw(r, viewX, viewY, viewScale, 0);
            ship.draw(r, viewX, viewY, viewScale);
            if (f % 5 == 0) r.flush();
        }
    }

    // Deployment ramp with real physics (gravity, brake toward the sink).
    ship.reset(400.0f, 250.0f);
    ship.chute = true;
    ship.chuteOpen = 0.0f;
    ship.velX = 0.2f;
    ship.velY = 0.3f;
    for (int i = 0; i < 300; i++) {
        ship.update();
        r.clear();
        t.draw(r, viewX, viewY, viewScale, 0);
        ship.draw(r, viewX, viewY, viewScale);
        if (i % 5 == 0) r.flush();
    }

    printf("parachute: open=%.2f velY=%.3f sink=%.3f\n",
           ship.chuteOpen, ship.velY, PARACHUTE_SINK);
    printf("OK: %d frames rendered to frames/\n", 50 + 60);
    return 0;
}
