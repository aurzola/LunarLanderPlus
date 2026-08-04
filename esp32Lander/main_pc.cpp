#include <cstdio>
#include "game.h"
#include "renderer_pc.h"

int main()
{
    RendererPC r((int)SCREEN_W, (int)SCREEN_H, "frames");
    Game g;

    g.draw(r);

    g.input.startPressed = true;
    g.update();
    g.input.startPressed = false;

    for (int i = 0; i < 2000; i++) {
        g.input.angle = -PI / 6.0f;
        g.input.thrust = (i > 200 && i < 800) ? 0.5f : 0.0f;
        g.update();
        if (i % 25 == 0) g.draw(r);
    }
    g.draw(r);

    printf("state=%d score=%d fuel=%.1f\n",
           g.state, g.score, g.ship.fuel);
    return 0;
}
