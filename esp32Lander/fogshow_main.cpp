#include <cstdio>
#include <cstdlib>
#include "fogshow.h"
#include "terrain.h"
#include "renderer_pc.h"
#include "config.h"

// TEMP: renders the 4-variant fog showcase to frames/fogshow_*.ppm
int main(int argc, char **argv)
{
    int seed = (argc > 1) ? atoi(argv[1]) : 1;
    srand(1000 + seed);

    RendererPC r((int)SCREEN_W, (int)SCREEN_H, "frames");
    Terrain t;
    t.generate(5);

    for (int i = 0; i <= 20; i++) {
        r.clear();
        FogShow::draw(r, t, (float)i * 0.5f);
        r.flush();
    }
    printf("OK: fogshow frames rendered to frames/\n");
    return 0;
}