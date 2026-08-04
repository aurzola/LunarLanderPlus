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
        g.input.angle = -0.25f;
        g.input.throttle = 0;
        g.update();
        if (i % 25 == 0) g.draw(r);
    }
    g.draw(r);

    printf("state=%d score=%d gas=%.1f collided=%d playing=%d\n",
           g.state, g.score, g.ship.getGas(), g.collided, (int)g.playing);
    return 0;
}
