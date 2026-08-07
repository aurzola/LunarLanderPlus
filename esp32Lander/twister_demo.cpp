#include <cstdio>
#include <cstdlib>
#include "twister.h"
#include "terrain.h"
#include "ship.h"
#include "renderer_pc.h"
#include "config.h"

// Visual demo of the Triton twister: the ship flies in, gets sucked toward the
// vortex and is either captured (smash against the ground) or flung out after a
// strong enough escape. Renders PPM frames to frames/.
int main(int argc, char **argv)
{
    int seed = (argc > 1) ? atoi(argv[1]) : 1;
    int level = (argc > 2) ? atoi(argv[2]) : 8;
    srand(1000 + seed);

    RendererPC r((int)SCREEN_W, (int)SCREEN_H, "frames");
    Terrain t;
    Twister tw;
    Ship s;

    t.generate(level);
    tw.reset(level, t);
    if (!tw.active()) {
        fprintf(stderr, "FAIL: twister did not activate (level %d is not Triton)\n", level);
        return 1;
    }

    const float viewScale = SCREEN_H / 700.0f;
    const float viewX = 0.0f, viewY = 0.0f;

    // Start inside the influence radius so the funnel visibly sucks the ship.
    float ox = (float)(rand() % 1000) / 1000.0f - 0.5f;
    float oy = (float)(rand() % 1000) / 1000.0f - 0.5f;
    s.reset(tw.coreX() + 60.0f + ox * 30.0f, tw.coreY(t) - 90.0f + oy * 30.0f);
    s.velX = 0.0f;
    s.velY = 0.0f;

    int frames = 900;
    int capturedFrames = 0;
    bool sawFunnel = false;
    bool smashed = false;
    for (int i = 0; i < frames; i++) {
        s.update();              // integrate position/gravity (as in Game)
        tw.update(GAME_DT);
        tw.apply(s, t);

        if (tw.captured() && s.posY >= tw.coreY(t) - 4.0f) {
            smashed = true;
            s.crash();
        }

        r.clear();
        t.draw(r, viewX, viewY, viewScale, 0);
        tw.draw(r, t, viewX, viewY, viewScale);
        if (!smashed) s.draw(r, viewX, viewY, viewScale);
        if (i % 5 == 0 || i == frames - 1) r.flush();

        if (tw.captured()) capturedFrames++;
        if (tw.strength() > 0) sawFunnel = true;
    }

    printf("twister level=%d active=%d strength=%.2f capturedFrames=%d smashed=%d t=%.0fs\n",
           level, tw.active() ? 1 : 0, tw.strength(), capturedFrames, smashed ? 1 : 0,
           frames * GAME_DT);
    if (!sawFunnel) {
        fprintf(stderr, "FAIL: no funnel rendered\n");
        return 1;
    }
    printf("OK: %d frames rendered to frames/\n", frames / 5);
    return 0;
}
