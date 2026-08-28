#include <cstdio>
#include <cstdlib>
#include "volcanoes.h"
#include "terrain.h"
#include "renderer_pc.h"
#include "config.h"

int main(int argc, char **argv)
{
    int seed = (argc > 1) ? atoi(argv[1]) : 1;
    int level = (argc > 2) ? atoi(argv[2]) : 8;
    srand(1000 + seed);

    RendererPC r((int)SCREEN_W, (int)SCREEN_H, "frames");
    Terrain t;
    Volcanoes v;

    t.generate(level);
    v.reset(level, t);
    for (int tries = 0; tries < 100 && !v.active(); tries++) v.reset(level, t);
    if (!v.active()) {
        fprintf(stderr, "FAIL: volcanoes did not activate (level %d is not Io)\n", level);
        return 1;
    }

    const float viewScale = SCREEN_H / 700.0f;
    const float viewX = 0.0f, viewY = 0.0f;

    int frames = 600;
    int maxAlive = 0;
    for (int i = 0; i < frames; i++) {
        v.update(GAME_DT);
        if (v.particlesAlive() > maxAlive) maxAlive = v.particlesAlive();

        r.clear();
        t.draw(r, viewX, viewY, viewScale, 0);
        v.draw(r, viewX, viewY, viewScale);
        if (i % 5 == 0 || i == frames - 1) r.flush();
    }

    printf("volcanoes level=%d active=%d maxAlive=%d t=%.0fs\n",
           level, v.active() ? 1 : 0, maxAlive, frames * GAME_DT);

    if (maxAlive == 0) {
        fprintf(stderr, "FAIL: no volcano particles were visible during the run\n");
        return 1;
    }
    printf("OK: %d frames rendered to frames/\n", frames / 5);
    return 0;
}
