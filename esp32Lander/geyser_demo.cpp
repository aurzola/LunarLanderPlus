#include <cstdio>
#include <cstdlib>
#include "geysers.h"
#include "terrain.h"
#include "renderer_pc.h"
#include "config.h"

int main(int argc, char **argv)
{
    int seed = (argc > 1) ? atoi(argv[1]) : 1;
    int level = (argc > 2) ? atoi(argv[2]) : 7;
    srand(1000 + seed);

    RendererPC r((int)SCREEN_W, (int)SCREEN_H, "frames");
    Terrain t;
    Geysers g;

    t.generate(level);
    g.reset(level, t);
    for (int tries = 0; tries < 100 && !g.active(); tries++) g.reset(level, t);
    if (!g.active()) {
        fprintf(stderr, "FAIL: geysers did not activate (level %d is not Enceladus)\n", level);
        return 1;
    }

    const float viewScale = SCREEN_H / 700.0f;
    const float viewX = 0.0f, viewY = 0.0f;

    int frames = 600;
    int maxAlive = 0;
    for (int i = 0; i < frames; i++) {
        g.update(GAME_DT);
        if (g.particlesAlive() > maxAlive) maxAlive = g.particlesAlive();

        r.clear();
        t.draw(r, viewX, viewY, viewScale, 0);
        g.draw(r, viewX, viewY, viewScale);
        if (i % 5 == 0 || i == frames - 1) r.flush();
    }

    printf("geysers level=%d active=%d maxAlive=%d t=%.0fs\n",
           level, g.active() ? 1 : 0, maxAlive, frames * GAME_DT);

    if (maxAlive == 0) {
        fprintf(stderr, "FAIL: no geyser particles were visible during the run\n");
        return 1;
    }
    printf("OK: %d frames rendered to frames/\n", frames / 5);
    return 0;
}
