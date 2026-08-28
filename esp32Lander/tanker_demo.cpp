#include <cstdio>
#include <cstdlib>
#include "game.h"
#include "renderer_pc.h"

int main(int argc, char **argv)
{
    int seed = (argc > 1) ? atoi(argv[1]) : 1;
    srand(seed);
    RendererPC r((int)SCREEN_W, (int)SCREEN_H, "frames");
    Game g;

    g.newGame();
    g.level = 2;
    g.terrain.generate(2);
    g.tanker.reset(2, g.terrain, 100.0f);

    if (!g.tanker.active) {
        printf("Tanker not active for seed %d\n", seed);
        return 1;
    }

    g.ship.reset(g.tanker.drogueX(), g.tanker.drogueY() + 5.0f);
    g.ship.fuel = 10.0f;
    g.ship.velX = 0.0f;
    g.ship.velY = 0.02f;
    g.introTimer = 0;

    printf("bodyX=%.1f bodyY=%.1f portY=%.1f drogue=(%.1f,%.1f)\n",
           g.tanker.bodyX, g.tanker.bodyY, g.tanker.portY,
           g.tanker.drogueX(), g.tanker.drogueY());

    bool dockedOnce = false;
    for (int i = 0; i < 700; i++) {
        g.update();
        if (g.tanker.docked && !dockedOnce) {
            dockedOnce = true;
            printf("  DOCKED! frame %d fuel=%.0f\n", i, g.ship.fuel);
            g.draw(r);
            r.flush();
            r.clear();
        }
        if (g.tanker.leaving) {
            printf("  REFUELED + LEAVING! frame %d\n", i);
            g.draw(r);
            r.flush();
            r.clear();
            break;
        }
    }

    g.draw(r);
    printf("final: active=%d docked=%d leaving=%d done=%d fuel=%.0f pos=(%.0f,%.0f)\n",
           g.tanker.active, g.tanker.docked, g.tanker.leaving, g.tanker.done,
           g.ship.fuel, g.ship.posX, g.ship.posY);
    return 0;
}
