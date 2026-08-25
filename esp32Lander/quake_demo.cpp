#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "quake.h"
#include "ship.h"
#include "terrain.h"
#include "renderer_pc.h"
#include "config.h"
#include "moons.h"

int main(int argc, char **argv)
{
    int seed = (argc > 1) ? atoi(argv[1]) : 1;
    int level = (argc > 2) ? atoi(argv[2]) : 8;
    srand(1000 + seed);

    RendererPC r((int)SCREEN_W, (int)SCREEN_H, "frames");
    Terrain t;
    Ship ship;
    Quake q;

    t.generate(level);
    ship.reset(200, 150);

    // Quakes only fire on Io.
    q.reset(level, t, ship);
    for (int tries = 0; tries < 100 && !q.active(); tries++) {
        level = 8;
        t.generate(level);
        q.reset(level, t, ship);
    }
    if (!q.active()) {
        fprintf(stderr, "FAIL: quake did not activate (level %d is not Io)\n", level);
        return 1;
    }
    printf("quake demo level=%d zones=%d\n", level, t.zoneCount());

    const float viewScale = SCREEN_H / 700.0f;

    // Phase A: the ship is high in the sky; the quake follows it and buckles
    // the open surface below (no pad involved).
    float padY = 0;
    float yAtBefore = t.yAt(ship.posX, 500.0f);
    int frames = 0;
    bool sawRumble = false;
    bool sawAStrike = false;
    for (int i = 0; i < 100 && !sawAStrike; i++) {
        q.update(1.0f, t, ship);
        if (q.phase() == Quake::RUMBLING) sawRumble = true;
        if (q.justStruck()) sawAStrike = true;
        float viewX = SCREEN_W * 0.5f - ship.posX * viewScale;
        float viewY = SCREEN_H * 0.6f - t.yAt(ship.posX, 500.0f) * viewScale;
        if (i == 0 || q.phase() == Quake::RUMBLING || q.justStruck() ||
            (sawAStrike && (i % 5) == 0)) {
            r.clear();
            t.draw(r, viewX, viewY, viewScale, 0);
            q.draw(r, viewX, viewY, viewScale);
            ship.draw(r, viewX, viewY, viewScale);
            r.flush();
            frames++;
        }
    }
    float yAtAfter = t.yAt(ship.posX, 500.0f);
    if (!sawAStrike) {
        fprintf(stderr, "FAIL: quake never struck in phase A\n");
        return 1;
    }
    printf("phase A: strike at x=%.1f surface %.1f -> %.1f\n",
           q.strikeX(), yAtBefore, yAtAfter);
    if (!t.isRupturedAt(q.strikeX())) {
        fprintf(stderr, "FAIL: phase A strike did not rupture the surface\n");
        return 1;
    }

    // Phase B: bring the ship down close to an unbroken pad. The next quake
    // strikes near the ship -> destroys that pad (label gone, not landable).
    int pad = -1;
    for (int z = 0; z < t.zoneCount(); z++) {
        if (!t.zoneBroken(z)) { pad = z; break; }
    }
    if (pad < 0) {
        fprintf(stderr, "FAIL: no pad left to destroy in phase B\n");
        return 1;
    }
    ship.posX = t.zoneLabelX(pad);
    padY = t.getLines()[t.zoneStart(pad)].y1;
    ship.posY = padY - 60.0f; // close to the surface, descending
    const std::vector<TerrainLine> &tl = t.getLines();
    int zs0 = t.zoneStart(pad);
    float labelBefore = tl[zs0].labelX;
    bool landableBefore = tl[zs0].landable;

    bool sawBStrike = false;
    for (int i = 0; i < 200 && !sawBStrike; i++) {
        q.update(1.0f, t, ship);
        if (q.justStruck()) sawBStrike = true;
        float viewX = SCREEN_W * 0.5f - ship.posX * viewScale;
        float viewY = SCREEN_H * 0.55f - padY * viewScale;
        if (q.phase() == Quake::RUMBLING || q.justStruck() ||
            (sawBStrike && (i % 5) == 0)) {
            r.clear();
            t.draw(r, viewX, viewY, viewScale, 0);
            q.draw(r, viewX, viewY, viewScale);
            ship.draw(r, viewX, viewY, viewScale);
            r.flush();
            frames++;
        }
    }
    if (!sawBStrike) {
        fprintf(stderr, "FAIL: quake never struck in phase B\n");
        return 1;
    }
    int brokenPad = q.rupturedZone();
    if (brokenPad < 0) {
        fprintf(stderr, "FAIL: phase B strike hit no pad (strike x=%.1f, ship x=%.1f)\n",
                q.strikeX(), ship.posX);
        return 1;
    }
    int bzs = t.zoneStart(brokenPad);
    printf("phase B: pad %d broken (label %.1f -> %.1f, landable %d -> %d)\n",
           brokenPad, labelBefore, tl[bzs].labelX, landableBefore ? 1 : 0,
           tl[bzs].landable ? 1 : 0);

    bool ok = t.zoneBroken(brokenPad) &&
              !tl[bzs].landable &&
              tl[bzs].labelX < 0 &&
              t.isRupturedAt(q.strikeX());
    if (!ok) {
        fprintf(stderr, "FAIL: pad not properly destroyed (label/landable/isRupturedAt)\n");
        return 1;
    }
    printf("OK: surface rupture + pad %d destroyed, %d frames rendered to frames/\n",
           brokenPad, frames);
    return 0;
}
