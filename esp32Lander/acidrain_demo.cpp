#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "acidrain.h"
#include "terrain.h"
#include "ship.h"
#include "renderer_pc.h"
#include "config.h"

int main(int argc, char **argv)
{
    int seed = (argc > 1) ? atoi(argv[1]) : 1;
    int level = (argc > 2) ? atoi(argv[2]) : 3;
    srand(1000 + seed);

    RendererPC r((int)SCREEN_W, (int)SCREEN_H, "frames");
    Terrain t;
    AcidRain a;
    Ship s;

    t.generate(level);
    a.reset(level, t);
    if (!a.active()) {
        fprintf(stderr, "FAIL: acid rain did not activate (level %d is not Europa)\n", level);
        return 1;
    }
    printf("acidrain level=%d active=%d cells=%d\n", level, a.active() ? 1 : 0, a.cellCount());

    const float viewScale = SCREEN_H / 700.0f;
    const float viewX = 0.0f, viewY = 0.0f;
    s.reset(150, 150);

    float maxMeter = 0.0f;
    int inRainFrames = 0, outRainFrames = 0;

    int frames = 600;
    for (int i = 0; i < frames; i++) {
        a.update(GAME_DT);

        float x = 40.0f + 720.0f * fmodf((float)i * 0.003f, 1.0f);
        s.posX = x;
        s.posY = 120.0f + sinf((float)i * 0.05f) * 60.0f;

        if (a.inRain(s.posX, s.posY)) {
            a.corrode();
            inRainFrames++;
        } else {
            a.dry();
            outRainFrames++;
        }

        float m = a.meterGet();
        if (m > maxMeter) maxMeter = m;

        r.clear();
        t.draw(r, viewX, viewY, viewScale, 0);
        a.draw(r, t, viewX, viewY, viewScale);
        s.draw(r, viewX, viewY, viewScale);
        a.drawSizzle(r, s.posX, s.posY, viewX, viewY, viewScale);

        if (i % 5 == 0 || i == frames - 1) r.flush();
    }

    printf("maxMeter=%.1f inRain=%d outRain=%d t=%.0fs\n",
           maxMeter, inRainFrames, outRainFrames, frames * GAME_DT);
    if (maxMeter < 0.3f) {
        fprintf(stderr, "FAIL: meter barely moved (max %.1f)\n", maxMeter);
        return 1;
    }
    if (inRainFrames == 0) {
        fprintf(stderr, "FAIL: ship was never in rain\n");
        return 1;
    }
    if (outRainFrames == 0) {
        fprintf(stderr, "FAIL: ship was never OUT of rain (dry not tested)\n");
        return 1;
    }
    printf("OK: %d frames rendered to frames/\n", frames / 5);
    return 0;
}
