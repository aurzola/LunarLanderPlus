#include <cstdio>
#include <cstdlib>
#include "atmosphere.h"
#include "terrain.h"
#include "ship.h"
#include "renderer_pc.h"
#include "config.h"

int main(int argc, char **argv)
{
    int seed = (argc > 1) ? atoi(argv[1]) : 1;
    int level = (argc > 2) ? atoi(argv[2]) : 6;
    srand(1000 + seed);

    RendererPC r((int)SCREEN_W, (int)SCREEN_H, "frames");
    Terrain t;
    Atmosphere a;
    Ship s;

    t.generate(level);
    a.reset(level);
    for (int tries = 0; tries < 100 && !a.active(); tries++) a.reset(level);
    if (!a.active()) {
        fprintf(stderr, "FAIL: atmosphere did not activate (level %d is not Titan)\n", level);
        return 1;
    }

    const float viewScale = SCREEN_H / 700.0f;
    const float viewX = 0.0f, viewY = 0.0f;
    s.reset(150, 150);

    int frames = 600;
    int hiddenFrames = 0;
    for (int i = 0; i < frames; i++) {
        a.update(GAME_DT);
        s.posY = 150.0f + (float)i * 0.6f;
        s.posX = 150.0f;

        r.clear();
        t.draw(r, viewX, viewY, viewScale, 0);
        a.drawSky(r, t, viewX, viewY, viewScale);
        bool hidden = a.hidesShip(s.posX, s.posY);
        if (!hidden) s.draw(r, viewX, viewY, viewScale);
        if (hidden) hiddenFrames++;
        if (i % 5 == 0 || i == frames - 1) r.flush();
    }

    printf("atmosphere level=%d active=%d bands=%d hiddenFrames=%d t=%.0fs\n",
           level, a.active() ? 1 : 0, a.bandCount(), hiddenFrames, frames * GAME_DT);
    if (hiddenFrames == 0) {
        fprintf(stderr, "FAIL: the ship was never hidden by the fog\n");
        return 1;
    }
    printf("OK: %d frames rendered to frames/\n", frames / 5);
    return 0;
}
