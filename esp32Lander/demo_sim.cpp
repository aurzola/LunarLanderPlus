#include <cstdio>
#include <cstdlib>
#include "game.h"
#include "config.h"

int main(int argc, char **argv)
{
    int seeds = 60;
    if (argc > 1) seeds = atoi(argv[1]);

    int wins = 0, losses = 0, timeouts = 0, stuck = 0;
    float sumTime = 0, sumWinTime = 0;

    for (int s = 0; s < seeds; s++) {
        srand(1000 + s);
        Game g;
        int steps = 0;
        bool won = false;

        while (g.state == STATE_WAITING && steps < 100000) {
            g.update();
            steps++;
        }
        if (g.state != STATE_PLAYING) {
            printf("seed %d: no demo started\n", s);
            timeouts++;
            continue;
        }

        // Detect the "hovering forever" bug: the ship stuck with VY=-2
        // (velY=-0.01, right at the autopilot's `velY < -0.01` thrust-cutoff).
        // A genuinely stuck ship keeps |velY| small in mid-air while thrust is
        // up. Count cumulative hover time; dump full state on the worst seeds.
        int hoverTicks = 0;
        int maxHover = 0;
        float hoverX = 0, hoverY = 0, hoverAlt = 0;
        float hoverThrust = 0, hoverAngle = 0, hoverVelX = 0, hoverVelY = 0;
        bool inPlume = false;

        while (steps < 600000) {
            g.update();
            steps++;
            if (g.state == STATE_LANDED) won = true;
            if (g.state == STATE_WAITING) break;

            if (g.state == STATE_PLAYING &&
                fabsf(g.ship.velY) < 0.02f &&
                g.ship.altitude > 30.0f &&
                g.input.thrust > 0.05f) {
                if (++hoverTicks > maxHover) {
                    maxHover = hoverTicks;
                    hoverX = g.ship.posX; hoverY = g.ship.posY;
                    hoverAlt = g.ship.altitude;
                    hoverThrust = g.input.thrust;
                    hoverAngle = g.input.angle;
                    hoverVelX = g.ship.velX; hoverVelY = g.ship.velY;
                    inPlume = g.geysers.inPlume(g.ship.posX, g.ship.posY);
                }
            } else {
                if (hoverTicks > 1500) {
                    printf("seed %d: HOVER %.0f ticks x=%.0f y=%.0f alt=%.0f "
                           "velX=%.3f velY=%.3f thrust=%.2f angle=%.0f plume=%d\n",
                           s, (float)hoverTicks, hoverX, hoverY, hoverAlt,
                           hoverVelX, hoverVelY, hoverThrust,
                           hoverAngle * 180.0f / PI, inPlume ? 1 : 0);
                }
                hoverTicks = 0;
            }
        }

        if (steps >= 600000) {
            timeouts++;
            printf("seed %d: TIMEOUT state=%d level=%d x=%.0f y=%.0f\n",
                   s, g.state, g.level, g.ship.posX, g.ship.posY);
            continue;
        }
        if (maxHover > 1500) {
            stuck++;
            printf("seed %d: STUCK-HOVER max=%d ticks level=%d\n",
                   s, maxHover, g.level);
        }

        float t = steps * GAME_DT;
        sumTime += t;
        if (won) { wins++; sumWinTime += t; }
        else losses++;
        printf("seed %d: %s t=%.0fs level=%d\n", s, won ? "WIN" : "LOSE", t, g.level);
    }

    printf("=== wins=%d losses=%d timeouts=%d stuck=%d winRate=%.0f%%\n",
           wins, losses, timeouts, stuck,
           (wins + losses) ? 100.0f * wins / (wins + losses) : 0);
    if (seeds) {
        printf("avg demo time %.0fs, avg win time %.0fs\n",
               sumTime / seeds, (wins ? sumWinTime / wins : 0));
    }
    return 0;
}
