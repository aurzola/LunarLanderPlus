#include <cstdio>
#include <cstdlib>
#include "game.h"
#include "config.h"

int main(int argc, char **argv)
{
    int seeds = 60;
    if (argc > 1) seeds = atoi(argv[1]);

    int wins = 0, losses = 0, timeouts = 0;
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

        while (steps < 600000) {
            g.update();
            steps++;
            if (g.state == STATE_LANDED) won = true;
            if (g.state == STATE_WAITING) break;
        }

        if (steps >= 600000) {
            timeouts++;
            printf("seed %d: TIMEOUT state=%d level=%d x=%.0f y=%.0f\n",
                   s, g.state, g.level, g.ship.posX, g.ship.posY);
            continue;
        }

        float t = steps * GAME_DT;
        sumTime += t;
        if (won) { wins++; sumWinTime += t; }
        else losses++;
        printf("seed %d: %s t=%.0fs level=%d\n", s, won ? "WIN" : "LOSE", t, g.level);
    }

    printf("=== wins=%d losses=%d timeouts=%d winRate=%.0f%%\n",
           wins, losses, timeouts,
           (wins + losses) ? 100.0f * wins / (wins + losses) : 0);
    if (seeds) {
        printf("avg demo time %.0fs, avg win time %.0fs\n",
               sumTime / seeds, (wins ? sumWinTime / wins : 0));
    }
    return 0;
}
