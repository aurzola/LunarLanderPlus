#include <cstdio>
#include <cstdlib>
#include "game.h"
#include "renderer_pc.h"

int main(int argc, char **argv)
{
    int seed = (argc > 1) ? atoi(argv[1]) : 1;
    srand(1000 + seed);
    RendererPC r((int)SCREEN_W, (int)SCREEN_H, "frames");
    Game g;
    int frame = 0;

    while (g.state == STATE_WAITING && frame < 100000) {
        g.update();
        if (frame % 15 == 0) g.draw(r);
        frame++;
    }
    while (g.state == STATE_PLAYING && frame < 200000) {
        g.update();
        if (frame % 12 == 0) g.draw(r);
        frame++;
    }
    g.draw(r);

    printf("state=%d level=%d score=%d t=%.0fs\n",
           g.state, g.level, g.score, frame * GAME_DT);
    return 0;
}
