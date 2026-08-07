#include <cstdio>
#include <cstdlib>
#include "rings.h"
#include "terrain.h"
#include "ship.h"
#include "renderer_pc.h"
#include "config.h"

int main(int argc, char **argv)
{
    int seed = (argc > 1) ? atoi(argv[1]) : 1;
    int level = (argc > 2) ? atoi(argv[2]) : 4;
    srand(1000 + seed);

    RendererPC r((int)SCREEN_W, (int)SCREEN_H, "frames");
    Terrain t;
    Rings rings;
    Ship s;

    t.generate(level);
    rings.reset(level, t);
    for (int tries = 0; tries < 100 && !rings.active(); tries++) rings.reset(level, t);
    if (!rings.active()) {
        fprintf(stderr, "FAIL: rings did not activate (level %d is not Ganymede)\n", level);
        return 1;
    }

    const float viewScale = SCREEN_H / 700.0f;
    const float viewX = 0.0f, viewY = 0.0f;
    s.reset(400, 300);

    int frames = 400;
    int hitFrames = 0;
    bool sawRocks = false;
    for (int i = 0; i < frames; i++) {
        rings.update(GAME_DT);

        r.clear();
        t.draw(r, viewX, viewY, viewScale, 0);
        rings.draw(r, t, viewX, viewY, viewScale);
        bool hit = rings.hitsShip(t, s.posX, s.posY, RING_SHIP_RADIUS);
        if (!hit) s.draw(r, viewX, viewY, viewScale);
        if (hit) hitFrames++;
        if (i % 5 == 0 || i == frames - 1) r.flush();

        float wx, wy;
        for (int k = 0; k < rings.ringCount() && !sawRocks; k++) {
            for (int j = 0; j < rings.rocksInRing(k); j++) {
                if (rings.rockVisible(t, k, j, wx, wy)) { sawRocks = true; break; }
            }
        }
    }

    printf("rings level=%d active=%d rocksVisible=%d hitFrames=%d t=%.0fs\n",
           level, rings.active() ? 1 : 0, sawRocks ? 1 : 0, hitFrames, frames * GAME_DT);
    if (!sawRocks) {
        fprintf(stderr, "FAIL: no ring rock appeared above the terrain\n");
        return 1;
    }
    printf("OK: %d frames rendered to frames/\n", frames / 5);
    return 0;
}
