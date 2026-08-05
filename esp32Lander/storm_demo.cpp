#include <cstdio>
#include <cstdlib>
#include "storm.h"
#include "terrain.h"
#include "ship.h"
#include "renderer_pc.h"
#include "config.h"

int main(int argc, char **argv)
{
    int seed = (argc > 1) ? atoi(argv[1]) : 1;
    int level = (argc > 2) ? atoi(argv[2]) : 9;
    if (level < STORM_START_LEVEL) level = STORM_START_LEVEL;
    srand(1000 + seed);

    RendererPC r((int)SCREEN_W, (int)SCREEN_H, "frames");
    Terrain t;
    Ship ship;
    Storm storm;

    t.generate(level);
    storm.reset(level);

    ship.reset(400.0f, 380.0f);
    ship.scale = 1.0f;

    const float viewScale = SCREEN_H / 700.0f;
    const float viewX = 0.0f, viewY = 0.0f;

    int frames = 1000;
    int maxAlive = 0;
    for (int i = 0; i < frames; i++) {
        storm.update(GAME_DT, t);
        if (storm.activeBolts() > maxAlive) maxAlive = storm.activeBolts();

        r.clear();
        storm.drawSky(r, viewX, viewY, viewScale);
        t.draw(r, viewX, viewY, viewScale, 0);
        ship.draw(r, viewX, viewY, viewScale);
        storm.drawBolts(r, viewX, viewY, viewScale);
        if (i % 5 == 0 || i == frames - 1) r.flush();
    }

    printf("storm level=%d active=%d bolts=%d maxAlive=%d t=%.0fs\n",
           level, storm.active() ? 1 : 0, storm.boltsSpawned(), maxAlive,
           frames * GAME_DT);

    if (storm.boltsSpawned() == 0) {
        fprintf(stderr, "FAIL: no bolts spawned\n");
        return 1;
    }
    if (maxAlive == 0) {
        fprintf(stderr, "FAIL: no bolt was visible during the run\n");
        return 1;
    }
    printf("OK: %d frames rendered to frames/\n", frames / 5);
    return 0;
}
