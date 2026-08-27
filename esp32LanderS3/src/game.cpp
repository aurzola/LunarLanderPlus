#include <cmath>
#include <cstdio>
#include <cstring>
#include "game.h"
#include "moons.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif
#if defined(ESP32)
#include <esp_random.h>
#endif

static const int TITLE_STAR_COUNT = 32;
static const int titleStars[][2] = {
    { 293,199 }, { 283,19 }, { 188,90 }, { 169,29 }, { 62,94 }, { 10,20 },
    { 226,90 }, { 163,155 }, { 115,14 }, { 249,125 }, { 209,131 }, { 308,77 },
    { 204,40 }, { 259,43 }, { 14,59 }, { 106,79 }, { 83,24 }, { 174,193 },
    { 55,205 }, { 20,89 }, { 9,170 }, { 218,205 }, { 256,199 }, { 110,188 },
    { 220,175 }, { 36,133 }, { 107,118 }, { 150,96 }, { 228,3 }, { 66,144 },
    { 274,106 }, { 82,60 },
};

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Refuel probe of the module: a thin boom (two rails) ending in a solid
// triangular arrowhead that must penetrate the tanker drogue basket. bx,by is
// the module center in screen space; ux,uy the probe direction (unit vector);
// scPx the pixel scale of the drawing (viewScale in the main view, PIP_SCALE
// inside the PiP window).
static void drawProbe(Renderer &r, float bx, float by, float ux, float uy,
                      float shipScale, float scPx)
{
    float px_ = -uy, py_ = ux;
    float inner = 4.0f * shipScale * scPx;
    float L = TANKER_NOZZLE_LEN * shipScale * scPx;
    float rail = 0.4f * shipScale * scPx;

    // thin refuel rod: a bright hairline with a dim offset for depth
    r.line(bx + inner * ux - rail * px_, by + inner * uy - rail * py_,
           bx + L * ux - rail * px_, by + L * uy - rail * py_);
    r.lineShade(bx + inner * ux + rail * px_, by + inner * uy + rail * py_,
                bx + L * ux + rail * px_, by + L * uy + rail * py_, 180);

    // small collar where the rod leaves the hull
    float colW = 0.9f * shipScale * scPx;
    r.line(bx + inner * ux - colW * px_, by + inner * uy - colW * py_,
           bx + inner * ux + colW * px_, by + inner * uy + colW * py_);

    // Solid triangular arrowhead at the physical contact point (distance L).
    // The bright base at L is the align point; the triangle points toward the
    // basket so the direction to guide is unmistakable. No bright tip circle.
    float tx = bx + L * ux, ty = by + L * uy;
    float tL = 1.6f * shipScale * scPx;
    float tW = 1.1f * shipScale * scPx;
    float apx = tx + tL * ux, apy = ty + tL * uy;
    r.line(tx, ty, apx, apy);
    r.line(apx, apy, tx - tW * px_, ty - tW * py_);
    r.line(apx, apy, tx + tW * px_, ty + tW * py_);
    // filled arrowhead
    int steps = (int)(tL + 0.5f);
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        float mx = tx + tL * t * ux;
        float my = ty + tL * t * uy;
        float w = tW * (1.0f - t);
        r.line(mx - w * px_, my - w * py_, mx + w * px_, my + w * py_);
    }
    r.pixelShade(tx, ty, 255);
}

// Fill `n` chars with random display garbage (digits + letters, occasionally a
// dash) to simulate a scrambled instrument readout after a lightning hit.
static void glitchChars(char *out, int n)
{
    static const char CH[] = "0123456789ABCDEFGHJKLMNPQRSTUVWXYZ-";
    for (int i = 0; i < n; i++) out[i] = CH[rand() % (sizeof(CH) - 1)];
    out[n] = 0;
}

Game::Game()
    : state(STATE_WAITING), score(0), level(1), fuel(FUEL_MAX), introTimer(0),
      demo(false), demoTimer(DEMO_START_DELAY),
      windEnabled(false), windStrength(0), windDir(1),
      hullIntegrity(100),
      viewX(0), viewY(0), viewScale(1.0f),
      zoomedIn(false), resetTimer(0), landMultiplier(1), landPerfect(false), landFuelBonus(0),
      demoSkill(1.0f), demoTargetX(0), demoTargetY(0),
      windPhase(0), windFlipTimer(0), stormHitTimer(0), fuelMaxTimer(0), chuteTooLowTimer(0), warpInT(0), recycledTimer(0), demoHoldAltitude(false),
      tankerZooming(false),
      lavaBurn(false), ringHit(false), twisterCrash(false), tankerCrash(false),
      acidBurn(false), quakeCrash(false), explosionInited(false),
      demoTankerPhase(0),
      bgBakedRev(0), bgBaked(false), bgAllocFailed(false)
{
    input.startPressed = false;
    input.angle = 0;
    input.thrust = 0;
    input.powerLevel = 0;
    input.chuteToggle = false;
    chuteAvailable = true;
    terrain.init();
    storm.reset(level);
    if (moonHasTitan(level) || moonHasTwister(level)) storm.setEnabled(false);
    geysers.reset(level, terrain);
    volcanoes.reset(level, terrain);
    atmosphere.reset(level);
    rings.reset(level, terrain);
    twister.reset(level, terrain);
    tanker.reset(level, terrain, ship.fuel);
    wormhole.disable();
    stormHitTimer = 0;
    setZoom(false);
    setupTitleShip();
}

void Game::newGame()
{
    level = START_LEVEL;
    score = 0;
    fuel = FUEL_MAX;
    ship.fuel = FUEL_MAX;
    hullIntegrity = 100;
    state = STATE_PLAYING;
    ship.reset(110, 150);
    setZoom(false);
    resetTimer = 0;
    introTimer = LEVEL_INTRO_TIME;
    ship.velX = 0.415f;
    chuteAvailable = true;
    if (level <= 1) terrain.init();
    else terrain.generate(level);
    windEnabled = (level >= WIND_START_LEVEL) &&
                  (rand() % 100) < WIND_CHANCE_PERCENT &&
                  !moonHasTwister(level) &&
                  !moonHasRings(level);
    spawnWind();
    storm.reset(level);
    if (moonHasTitan(level) || moonHasTwister(level)) storm.setEnabled(false);
    geysers.reset(level, terrain);
    volcanoes.reset(level, terrain);
    atmosphere.reset(level);
    rings.reset(level, terrain);
    twister.reset(level, terrain);
    tanker.reset(level, terrain, ship.fuel);
    acidrain.reset(level, terrain);
    quake.reset(level, terrain, ship);
    spawnWormhole();
    if (wormhole.active()) isolateForWormhole();
    stormHitTimer = 0;
    recycledTimer = 0;
    lavaBurn = false;
    ringHit = false;
    tankerCrash = false;
    acidBurn = false;
    quakeCrash = false;
    explosionInited = false;
    terrain.clearCrater();
}

void Game::restartLevel()
{
    float f = ship.fuel;
    ship.reset(110, 150);
    ship.fuel = f;
    setZoom(false);
    resetTimer = 0;
    introTimer = LEVEL_INTRO_TIME;
    lavaBurn = false;
    ringHit = false;
    tankerCrash = false;
    acidBurn = false;
    quakeCrash = false;
    explosionInited = false;
    twisterCrash = false;
    wormhole.disable();
    recycledTimer = 0;
    terrain.clearCrater();

    if (state == STATE_GAMEOVER || state == STATE_WAITING) {
        state = STATE_WAITING;
        setupTitleShip();
    } else {
        state = STATE_PLAYING;
    }
}

void Game::nextLevel()
{
    level++;
    float f = ship.fuel;
    terrain.generate(level);
    windEnabled = (level >= WIND_START_LEVEL) &&
                  (rand() % 100) < WIND_CHANCE_PERCENT &&
                  !moonHasTwister(level) &&
                  !moonHasRings(level);
    spawnWind();
    storm.reset(level);
    if (moonHasTitan(level) || moonHasTwister(level)) storm.setEnabled(false);
    geysers.reset(level, terrain);
    volcanoes.reset(level, terrain);
    atmosphere.reset(level);
    rings.reset(level, terrain);
    twister.reset(level, terrain);
    tanker.reset(level, terrain, ship.fuel);
    acidrain.reset(level, terrain);
    quake.reset(level, terrain, ship);
    spawnWormhole();
    if (wormhole.active()) isolateForWormhole();
    stormHitTimer = 0;
    lavaBurn = false;
    ringHit = false;
    tankerCrash = false;
    acidBurn = false;
    quakeCrash = false;
    explosionInited = false;
    terrain.clearCrater();
    state = STATE_PLAYING;
    ship.reset(110, 150);
    ship.fuel = f;
    setZoom(false);
    resetTimer = 0;
    introTimer = LEVEL_INTRO_TIME;
    ship.velX = 0.415f;
}

void Game::endGame()
{
    state = STATE_GAMEOVER;
    resetTimer = GAMEOVER_RESET_DELAY;
}

void Game::startDemo()
{
    demo = true;
    demoHoldAltitude = false;
#if defined(ESP32)
    srand(esp_random());
#endif
    if (rand() % 100 < 50) demoSkill = (float)(rand() % 36) / 100.0f;
    else demoSkill = 0.6f + (float)(rand() % 41) / 100.0f;
    // Demo level: random 1..DEMO_MAX_LEVEL every cycle (DEMO_LEVEL_FORCE
    // pins it for testing, DEMO_LEVEL_FIRST > 0 would fix the opener).
    level = (DEMO_LEVEL_FORCE > 0) ? DEMO_LEVEL_FORCE
                                   : (DEMO_LEVEL_FIRST > 0) ? DEMO_LEVEL_FIRST
                                                            : 1 + rand() % DEMO_MAX_LEVEL;

    const bool firstLevelShowcase = DEMO_LEVEL_FIRST > 0 && DEMO_LEVEL_FORCE <= 0;

    // Wormhole showcase: while DEMO_WORMHOLE_FIRST is on, the very first demo
    // level opens the sky wormhole, so re-roll until the level can host one
    // (>= WORMHOLE_START_LEVEL and on an effect-free moon, so the wormhole
    // never shares the sky with another effect). Never runs together with the
    // first-level showcase (both would fight over the same demo cycle).
    const bool showcase = DEMO_WORMHOLE_FIRST && DEMO_LEVEL_FORCE <= 0 && !firstLevelShowcase;
    if (showcase) {
        while (level < WORMHOLE_START_LEVEL || !moonEffectFree(level))
            level = 1 + rand() % DEMO_MAX_LEVEL;
    }
    score = 0;
    float savedHull = demo ? hullIntegrity : 100.0f;
    hullIntegrity = savedHull;
    state = STATE_PLAYING;
    if (level <= 1) terrain.init();
    else terrain.generate(level);
    windEnabled = (level >= WIND_START_LEVEL) &&
                  (rand() % 100) < WIND_CHANCE_PERCENT &&
                  !moonHasTwister(level) &&
                  !moonHasRings(level) &&
                  !showcase;
    spawnWind();
    storm.reset(level);
    if (showcase || moonHasTitan(level) || moonHasTwister(level)) storm.setEnabled(false);
    geysers.reset(level, terrain);
    if (showcase) geysers.setEnabled(false);
    volcanoes.reset(level, terrain);
    if (showcase) volcanoes.setEnabled(false);
    atmosphere.reset(level);
    if (showcase) atmosphere.setEnabled(false);
    rings.reset(level, terrain);
    if (showcase) rings.setEnabled(false);
    twister.reset(level, terrain);
    if (showcase) twister.setEnabled(false);
    tanker.reset(level, terrain, ship.fuel);
    acidrain.reset(level, terrain);
    if (showcase) acidrain.setEnabled(false);
    quake.reset(level, terrain, ship);
    if (showcase) quake.setEnabled(false);
    // Demo showcase: jump-start the acid meter so the attract shows the
    // corrosion crash quickly (the ship slowly dissolves in the rain and
    // the player sees the "ACID RAIN CORRODED THE SHIP" ending).
    if (demo && !showcase && acidrain.active()) acidrain.setMeter(97.0f);
    stormHitTimer = 0;
    recycledTimer = 0;
    chuteAvailable = true;
    lavaBurn = false;
    ringHit = false;
    tankerCrash = false;
    acidBurn = false;
    quakeCrash = false;
    explosionInited = false;
    terrain.clearCrater();
    // Random initial altitude: the demo ship always spawns at a variable
    // height within the band, so each attract run starts differently.
    {
        float savedFuel = ship.fuel;
        int spanX = (int)(DEMO_SPAWN_X_MAX - DEMO_SPAWN_X_MIN);
        int spanY = (int)(DEMO_SPAWN_Y_MAX - DEMO_SPAWN_Y_MIN);
        float sx = DEMO_SPAWN_X_MIN + (float)(rand() % spanX);
        float sy = DEMO_SPAWN_Y_MIN + (float)(rand() % spanY);
        printf("[demo] startDemo level=%d savedFuel=%.1f spawn=(%.0f,%.0f)\n",
                      level, savedFuel, sx, sy);
        ship.reset(sx, sy);
        ship.fuel = savedFuel;
        printf("[demo] after restore ship.fuel=%.1f\n", ship.fuel);
    }
    ship.velX = 0.06f;
    setZoom(false);
    viewX = -ship.posX * viewScale + SCREEN_W * 0.5f;
    viewY = -ship.posY * viewScale + SCREEN_H * 0.5f;
    resetTimer = 0;
    introTimer = LEVEL_INTRO_TIME;
    spawnWormhole(DEMO_WORMHOLE_FIRST);
    setupDemoTarget();
}

void Game::setupDemoTarget()
{
    const std::vector<TerrainLine> &tl = terrain.getLines();

    // Wormhole showcase: the hole opened at any random sky point (same rules
    // as a real level). Spawn the ship beside it, outside the no-return zone
    // but well inside the pull zone, drifting toward it — so the radial pull,
    // the vortex and the swallow all play out on the CRT without a long
    // cross-country flight that the landing autopilot would lose to terrain.
    // Full authority and no jitter: a clean, reliable showcase.
    if (demo && wormhole.active()) {
        float cx = wormhole.coreX();
        float cy = wormhole.coreY();
        float side = (rand() % 2) ? 1.0f : -1.0f;
        float sx = cx + side * 150.0f;
        if (sx < 30.0f || sx > terrain.getWidth() - 30.0f) {
            side = -side;
            sx = cx + side * 150.0f;
        }
        ship.reset(sx, cy);
        ship.scale = 1.5f; // normal view scale (Ship::reset() zeroes it)
        ship.velX = side * 0.2f;
        ship.velY = 0.0f;
        demoTargetX = cx;
        demoTargetY = cy;
        demoSkill = 1.0f;
        return;
    }

    // Aim the demo at the tanker's underside drogue when one is present, so the
    // autopilot flies up to it and plugs the probe in (aerial refueling). The
    // runDemoAI tanker mode tracks the swaying drogue live.
    if (tanker.active) {
        demoTargetX = tanker.drogueX();
        demoTargetY = tanker.drogueY() + TANKER_NOZZLE_LEN * ship.scale;
        demoSkill = 0.85f;
        demoTankerPhase = 0;
    } else
    // TEST aim: prefer a lava-covered strip of a landing pad (Io), so the
    // burnt-ship ending shows up while tuning it. Pick the lava zone closest
    // to the spawn so the flight is short and cannot land short on an
    // intervening pad. Regenerate the forced level until lava is available;
    // otherwise fall back to any landing pad.
    {
    int lavaPick = -1;
    for (int attempt = 0; attempt < 20 && lavaPick < 0; attempt++) {
        float bestDist = 1e9f;
        for (int i = 0; i < volcanoes.lavaRangeCount(); i++) {
            if (volcanoes.lavaRangeX2(i) - volcanoes.lavaRangeX1(i) < 8.0f) continue;
            float mid = (volcanoes.lavaRangeX1(i) + volcanoes.lavaRangeX2(i)) * 0.5f;
            float d = fabsf(mid - ship.posX);
            if (d < bestDist) {
                bestDist = d;
                lavaPick = i;
            }
        }
        if (lavaPick >= 0 || DEMO_LEVEL_FORCE <= 0) break;
        terrain.generate(level);
        storm.reset(level);
        if (moonHasTitan(level) || moonHasTwister(level)) storm.setEnabled(false);
        geysers.reset(level, terrain);
        volcanoes.reset(level, terrain);
        atmosphere.reset(level);
        rings.reset(level, terrain);
        twister.reset(level, terrain);
    }

    if (lavaPick >= 0) {
        demoTargetX = (volcanoes.lavaRangeX1(lavaPick) + volcanoes.lavaRangeX2(lavaPick)) * 0.5f;
        demoTargetY = 500.0f;
        for (int i = 0; i < (int)tl.size(); i++) {
            if (demoTargetX >= tl[i].x1 && demoTargetX <= tl[i].x2) {
                demoTargetY = tl[i].y1;
                break;
            }
        }
        demoSkill = 0.85f;
    } else {
        std::vector<float> cx, cy;
        for (int i = 0; i < (int)tl.size(); i++) {
            if (tl[i].labelX >= 0) {
                cx.push_back(tl[i].labelX);
                cy.push_back(tl[i].y1);
            }
        }
        int pick = (int)cx.size() ? rand() % (int)cx.size() : 0;
        demoTargetX = cx[pick];
        demoTargetY = cy[pick];
        if (demoSkill < 0.35f) {
            float off = ((float)(rand() % 200) / 100.0f - 1.0f) * (0.35f - demoSkill) * 110.0f;
            demoTargetX += off;
        }
    }
    }
}

void Game::endDemoToTitle()
{
    printf("[demo] endDemoToTitle ship.fuel=%.1f hullIntegrity=%.1f\n",
                  ship.fuel, hullIntegrity);
    float savedFuel = ship.fuel;
    state = STATE_WAITING;
    demoTimer = DEMO_START_DELAY;
    terrain.init();
    setZoom(false);
    wormhole.disable();
    setupTitleShip();
    ship.fuel = savedFuel;
    printf("[demo] after setupTitleShip+restore ship.fuel=%.1f\n", ship.fuel);
}

void Game::spawnWormhole(bool force)
{
    wormhole.disable();
    if (level < WORMHOLE_START_LEVEL) return;
    // The wormhole never combines with any other effect: it only opens on a
    // moon without an ambient effect of its own (LUNA/EUROPA/CALLISTO).
    if (!moonEffectFree(level)) return;
    if (demo) {
        // Attract mode: only the showcase first level opens a sky wormhole.
        // Same placement rules as a real level (any random point of the sky,
        // clamped above the terrain profile); setupDemoTarget() spawns the
        // ship beside it so the pull, vortex and swallow play out. After the
        // swallow the demo just returns to the title.
        if (!force) return;
        float cx = WORMHOLE_SKY_X_MARGIN +
                   (float)(rand() % (int)(terrain.getWidth() - 2.0f * WORMHOLE_SKY_X_MARGIN));
        float cy = WORMHOLE_SKY_Y_MIN +
                   (float)(rand() % (int)(WORMHOLE_SKY_Y_MAX - WORMHOLE_SKY_Y_MIN));
        float gy = terrain.yAt(cx, 500.0f);
        if (cy > gy - WORMHOLE_SKY_CLEAR) cy = gy - WORMHOLE_SKY_CLEAR;
        wormhole.reset(cx, cy);
        return;
    }
    if (rand() % 100 >= WORMHOLE_CHANCE_PERCENT) return;
    // Any position of the sky: random x over the world and y within the sky
    // band, clamped so the nucleus stays clear of the terrain profile.
    float cx = WORMHOLE_SKY_X_MARGIN +
               (float)(rand() % (int)(terrain.getWidth() - 2.0f * WORMHOLE_SKY_X_MARGIN));
    float cy = WORMHOLE_SKY_Y_MIN +
               (float)(rand() % (int)(WORMHOLE_SKY_Y_MAX - WORMHOLE_SKY_Y_MIN));
    float gy = terrain.yAt(cx, 500.0f);
    if (cy > gy - WORMHOLE_SKY_CLEAR) cy = gy - WORMHOLE_SKY_CLEAR;
    wormhole.reset(cx, cy);
}

void Game::isolateForWormhole()
{
    // While a wormhole is present every other effect and the tanker stay off.
    if (!wormhole.active()) return;
    windEnabled = false;
    storm.setEnabled(false);
    geysers.setEnabled(false);
    volcanoes.setEnabled(false);
    atmosphere.setEnabled(false);
    rings.setEnabled(false);
    twister.setEnabled(false);
    tanker.setEnabled(false);
    acidrain.setEnabled(false);
    quake.setEnabled(false);
}

void Game::wormholeJump()
{
    int cur = moonIndex(level);
    int nidx = cur;
    while (nidx == cur) nidx = rand() % 8;
    // The cycle order is not the identity: jump to the SLOT that plays moon
    // nidx. nextLevel() does level++ first -> lands on moon nidx.
    level = 8 + moonSlotOfIndex(nidx);
    nextLevel();
    // No level intro on a teleport: the new moon starts playing right away and
    // the ship materializes with a fade-in (warpInT ramps ship.scale 0->1.5).
    introTimer = 0;
    // Reappear at any position between the sky and the terrain.
    float w = terrain.getWidth();
    ship.posX = 60.0f + (float)(rand() % (int)(w - 120.0f));
    float gy = terrain.yAt(ship.posX, 500.0f);
    float yMin = 90.0f;
    float yMax = gy - 40.0f;
    if (yMax < yMin) yMax = yMin;
    ship.posY = yMin + (float)(rand() % (int)(yMax - yMin + 1));
    ship.velX = (float)(rand() % 41) / 100.0f - 0.2f;
    ship.velY = 0.0f;
    ship.rotation = 0.0f;
    ship.targetRotation = 0.0f;
    // Start invisible: the warp-in ramp grows ship.scale 0->1.5 over the next
    // frames (avoids a one-frame flash of a full-size ship before the fade).
    ship.scale = 0.0f;
    warpInT = WORMHOLE_WARP_IN_T;
    recycledTimer = WORMHOLE_RECYCLED_T; // "YOU'VE BEEN RECYCLED" banner
}

void Game::setupTitleShip()
{
    float fuelBefore = ship.fuel;
    ship.reset(110, 150);
    printf("[demo] setupTitleShip: reset wiped fuel %.1f -> %.1f\n",
                  fuelBefore, ship.fuel);
    ship.velX = -0.35f;
    ship.posX = (SCREEN_W - 20.0f) / viewScale;
}

void Game::runDemoAI()
{
    // Cruise phase after aerial refueling: descend toward the pad while flying
    // horizontally, then hand over to the normal descent controller.
    if (demoHoldAltitude) {
        float errX = demoTargetX - ship.posX;
        float desVX = clampf(errX * 0.004f, -0.12f, 0.12f);
        if (fabsf(errX) < 60.0f) desVX = clampf(errX * 0.002f, -0.04f, 0.04f);
        float windPush = ship.windStrength * WIND_ACCEL;
        float aX = clampf((desVX - ship.velX) * 0.02f - (float)ship.windDir * windPush,
                          -0.0015f, 0.0015f);

        float errY = ship.posY - demoTargetY;
        float desVY = clampf(-errY * 0.008f, -0.12f, 0.08f);
        float aY = clampf((ship.velY - desVY) * 0.03f + ship.gravity, 0.0f, 0.0018f);

        float thrust = sqrtf(aX * aX + aY * aY) / THRUST_ACCEL;
        float angle = atan2f(aX, aY) * 180.0f / PI;
        if (thrust > 1.0f) thrust = 1.0f;

        if (thrust < 0.015f) thrust = 0.0f;

        float imp = 1.0f - demoSkill;
        float n = (float)(rand() % 1001) / 1000.0f - 0.5f;
        angle += n * imp * 14.0f;
        if (thrust > 0.0f)
            thrust = clampf(thrust + n * imp * 0.05f, 0.0f, 1.0f);
        else
            thrust = 0.0f;

        float pulse = sinf((float)ship.counter * 0.04f) * 0.06f;
        thrust = clampf(thrust + pulse, 0.0f, 1.0f);

        float ta = clampf(angle, -90.0f, 90.0f) * (PI / 180.0f);
        input.angle += (ta - input.angle) * DEMO_ANGLE_SMOOTH;
        input.thrust = thrust;
        float pw = input.powerLevel;
        float step = DEMO_POWER_RATE * GAME_DT;
        if (thrust > pw) pw = fminf(thrust, pw + step);
        else pw = fmaxf(thrust, pw - step);
        input.powerLevel = pw;
        if (fabsf(errX) < 60.0f) demoHoldAltitude = false;
        return;
    }

    // TEMP crash showcase (DEMO_FORCE_TANKER_CRASH): fly the module straight
    // into the tanker hull. Instead of the careful docking approach, the
    // autopilot aims just past the hull centre and keeps full authority, so
    // the probe crosses the hull box and triggers the fuel explosion.
    if (DEMO_FORCE_TANKER_CRASH && demoTankerPhase < 0 &&
        tanker.active && !tanker.done) {
        float dir = (tanker.bodyX > ship.posX) ? 1.0f : -1.0f;
        float tx = tanker.bodyX + dir * TANKER_HULL_W; // plow through the hull
        float ty = tanker.bodyY;

        float errX = tx - ship.posX;
        float desVX = clampf(errX * 0.004f, -0.16f, 0.16f);
        float aX = clampf((desVX - ship.velX) * 0.02f, -0.002f, 0.002f);

        float errY = ty - ship.posY;
        float desVY = clampf(errY * 0.005f, -0.10f, 0.06f);
        float aY = clampf((ship.velY - desVY) * 0.03f + ship.gravity, 0.0f, 0.0018f);

        float thrust = sqrtf(aX * aX + aY * aY) / THRUST_ACCEL;
        float angle = atan2f(aX, aY) * 180.0f / PI;
        if (thrust > 1.0f) thrust = 1.0f;

        float ta = clampf(angle, -90.0f, 90.0f) * (PI / 180.0f);
        input.angle += (ta - input.angle) * DEMO_ANGLE_SMOOTH;
        input.thrust = thrust;
        float pw = input.powerLevel;
        float step = DEMO_POWER_RATE * GAME_DT;
        if (thrust > pw) pw = fminf(thrust, pw + step);
        else pw = fmaxf(thrust, pw - step);
        input.powerLevel = pw;
        return;
    }

    // Aerial-tanker mode: descend at a pre-position left of the drogue (so the
    // descent never crosses the hull band), then slide in horizontally at the
    // drogue altitude (below the hull) and plug the probe into the basket. The
    // target tracks the swaying drogue live (probe-and-drogue). Once docked,
    // the autopilot keeps making tiny corrections so the 1-second lock holds
    // and fuel keeps flowing.
    if (demoTankerPhase >= 0 && (tanker.targeted() || tanker.docked)) {
        float tx = tanker.drogueX();
        float ty = tanker.drogueY() + TANKER_NOZZLE_LEN * ship.scale;

        // When already docked, the autopilot actively cancels the horizontal
        // joystick offset to keep the probe centered in the drogue.
        if (tanker.docked) {
            float ox = tanker.dockOffsetX;
            float ang = (ox > 0.0f) ? -PI * 0.4f : (ox < 0.0f ? PI * 0.4f : 0.0f);
            input.angle += (ang - input.angle) * DEMO_ANGLE_SMOOTH;
            input.thrust = 0.3f;
            float pw = input.powerLevel;
            float step = DEMO_POWER_RATE * GAME_DT;
            if (input.thrust > pw) pw = fminf(input.thrust, pw + step);
            else pw = fmaxf(input.thrust, pw - step);
            input.powerLevel = pw;
            return;
        }

        if (demoTankerPhase == 0) {
            tx = tx - TANKER_APPROACH_X;
            if (fabsf(ship.posX - tx) < 8.0f && fabsf(ship.posY - ty) < 12.0f)
                demoTankerPhase = 1;
        }
        float errX = tx - ship.posX;
        float desVX = clampf(errX * 0.004f, -0.14f, 0.14f);
        if (fabsf(errX) < 50.0f) desVX = clampf(errX * 0.002f, -0.03f, 0.03f);
        float windPush = ship.windStrength * WIND_ACCEL;
        float aX = clampf((desVX - ship.velX) * 0.02f - (float)ship.windDir * windPush,
                          -0.0016f, 0.0016f);

        float errY = ship.posY - ty;
        float desVY = clampf(-errY * 0.005f, -0.06f, 0.05f);
        float aY = clampf((ship.velY - desVY) * 0.03f + ship.gravity, 0.0f, 0.0018f);

        float thrust = sqrtf(aX * aX + aY * aY) / THRUST_ACCEL;
        float angle = atan2f(aX, aY) * 180.0f / PI;
        if (thrust > 1.0f) thrust = 1.0f;

        if (thrust < 0.015f) thrust = 0.0f;

        float imp = 1.0f - demoSkill;
        float n = (float)(rand() % 1001) / 1000.0f - 0.5f;
        angle += n * imp * 14.0f;
        if (thrust > 0.0f)
            thrust = clampf(thrust + n * imp * 0.05f, 0.0f, 1.0f);
        else
            thrust = 0.0f;

        float pulse = sinf((float)ship.counter * 0.04f) * 0.06f;
        thrust = clampf(thrust + pulse, 0.0f, 1.0f);

        float ta = clampf(angle, -90.0f, 90.0f) * (PI / 180.0f);
        input.angle += (ta - input.angle) * DEMO_ANGLE_SMOOTH;
        input.thrust = thrust;
        float pw = input.powerLevel;
        float step = DEMO_POWER_RATE * GAME_DT;
        if (thrust > pw) pw = fminf(thrust, pw + step);
        else pw = fmaxf(thrust, pw - step);
        input.powerLevel = pw;
        return;
    }

    float errX = demoTargetX - ship.posX;
    float distX = fabsf(errX);
    float alt = ship.altitude;

    float desVX = clampf(errX * 0.003f, -0.10f, 0.10f);
    if (distX < 40.0f) desVX = clampf(errX * 0.002f, -0.04f, 0.04f);
    float windPush = ship.windStrength * WIND_ACCEL;
    float aX = clampf((desVX - ship.velX) * 0.02f - (float)ship.windDir * windPush,
                      -0.002f, 0.002f);

    float desVY = (distX > 50.0f) ? ((alt < 100.0f) ? 0.04f : 0.12f) : 0.03f;
    float aY = clampf((ship.velY - desVY) * 0.03f, 0.0f, 0.00075f);

    if (alt < 12.0f) aX *= 0.6f;
    if (alt < 2.5f) aX *= 0.05f;

    float thrust = sqrtf(aX * aX + aY * aY) / THRUST_ACCEL;
    float angle = atan2f(aX, aY) * 180.0f / PI;
    if (thrust > 1.0f) thrust = 1.0f;

    if (ship.velY < -0.01f) thrust = 0.0f;
    if (thrust < 0.015f) thrust = 0.0f;

    float imp = 1.0f - demoSkill;
    float n = (float)(rand() % 1001) / 1000.0f - 0.5f;
    angle += n * imp * 40.0f;
    if (thrust > 0.0f)
        thrust = clampf(thrust + n * imp * 0.08f, 0.0f, 1.0f);
    else
        thrust = 0.0f;

    float pulse = sinf((float)ship.counter * 0.04f) * 0.06f;
    thrust = clampf(thrust + pulse, 0.0f, 1.0f);

    float ta = clampf(angle, -90.0f, 90.0f) * (PI / 180.0f);
    input.angle += (ta - input.angle) * DEMO_ANGLE_SMOOTH;
    input.thrust = thrust;
    float pw = input.powerLevel;
    float step = DEMO_POWER_RATE * GAME_DT;
    if (thrust > pw) pw = fminf(thrust, pw + step);
    else pw = fmaxf(thrust, pw - step);
    input.powerLevel = pw;
}

static float terrainYAt(const std::vector<TerrainLine> &tl, float x, float fallback)
{
    for (int i = 0; i < (int)tl.size(); i++) {
        const TerrainLine &l = tl[i];
        if (x >= l.x1 && x <= l.x2 && l.x2 != l.x1) {
            float t = (x - l.x1) / (l.x2 - l.x1);
            return l.y1 + (l.y2 - l.y1) * t;
        }
    }
    return fallback;
}

void Game::spawnWind()
{
    windStreaks.clear();
    if (!windEnabled) return;

    float w = terrain.getWidth();
    float top = 9999;
    const std::vector<TerrainLine> &tl = terrain.getLines();
    for (int i = 0; i < (int)tl.size(); i++) {
        if (tl[i].y1 < top) top = tl[i].y1;
    }

    for (int i = 0; i < (int)WIND_STREAK_COUNT; i++) {
        WindStreak s;
        s.x = (float)(rand() % (int)(w * 10.0f)) / 10.0f;
        s.y = (float)(rand() % (int)(top - 60.0f)) + 20.0f;
        s.vy = ((float)(rand() % 1201) / 100.0f - 6.0f);
        s.f1 = 0.55f + (float)(rand() % 45) / 100.0f;
        s.f2 = 0.55f + (float)(rand() % 45) / 100.0f;
        windStreaks.push_back(s);
    }
}

void Game::spawnDust()
{
    dust.clear();
    if (!windEnabled) return;

    float w = terrain.getWidth();
    const std::vector<TerrainLine> &tl = terrain.getLines();
    for (int i = 0; i < DUST_COUNT; i++) {
        DustParticle d;
        d.x = ship.posX + ((float)(rand() % (int)(2.0f * DUST_RANGE * 10.0f)) / 10.0f - DUST_RANGE);
        while (d.x < 0) d.x += w;
        while (d.x > w) d.x -= w;
        d.y = terrainYAt(tl, d.x, 480.0f) - ((float)(rand() % 350) / 10.0f + 3.0f);
        d.vy = (float)(rand() % 401) / 100.0f - 2.0f;
        d.life = DUST_LIFE * (0.5f + (float)(rand() % 50) / 100.0f);
        dust.push_back(d);
    }
}

void Game::updateWind(float dt)
{
    if (!windEnabled) {
        windStreaks.clear();
        dust.clear();
        return;
    }
    if (windStreaks.empty()) spawnWind();
    if (dust.empty()) spawnDust();

    windPhase += dt;
    windFlipTimer -= dt;
    if (windFlipTimer <= 0) {
        windFlipTimer = 8.0f + (float)(rand() % 120) / 10.0f;
        if (rand() % 2) windDir = -windDir;
    }

    float gust = 0.5f + 0.5f * sinf(windPhase * 0.6f);
    windStrength = WIND_MIN + (1.0f - WIND_MIN) * gust;

    float top = 9999;
    const std::vector<TerrainLine> &tl = terrain.getLines();
    for (int i = 0; i < (int)tl.size(); i++) {
        if (tl[i].y1 < top) top = tl[i].y1;
    }

    float speed = (float)windDir * windStrength * WIND_STREAK_SPEED * dt;
    float w = terrain.getWidth() + 40.0f;
    float skyTop = (0.0f - viewY) / viewScale;
    float skyBot = (SCREEN_H * 0.55f - viewY) / viewScale;
    if (skyTop < 0.0f) skyTop = 0.0f;
    if (skyBot <= skyTop) skyBot = skyTop + 1.0f;
    for (int i = 0; i < (int)windStreaks.size(); i++) {
        WindStreak &s = windStreaks[i];
        s.x += speed;
        s.y += s.vy * dt;
        if (s.x > w) s.x -= w;
        else if (s.x < 0) s.x += w;
        float sBot = skyBot;
        float gy = terrainYAt(tl, s.x, 480.0f);
        if (gy - 4.0f < sBot) sBot = gy - 4.0f;
        if (sBot <= skyTop) sBot = skyTop + 1.0f;
        if (s.y < skyTop || s.y > sBot) {
            s.y = skyTop + ((float)(rand() % 1000) / 1000.0f) * (sBot - skyTop);
            s.vy = (float)(rand() % 1201) / 100.0f - 6.0f;
        }
    }

    float dustSpeed = speed * DUST_SPEED;
    float gw = terrain.getWidth();
    float nearF = landingProximity();
    for (int i = 0; i < (int)dust.size(); i++) {
        DustParticle &d = dust[i];
        d.life -= dt;
        d.x += dustSpeed + d.vy * dt * 0.2f;
        d.y += d.vy * dt;
        if (d.x > gw) d.x -= gw;
        else if (d.x < 0) d.x += gw;
        float gy = terrainYAt(tl, d.x, 480.0f);
        if (d.y > gy - 2.0f) d.y = gy - 2.0f;
        if (d.life <= 0) {
            d.x = ship.posX + ((float)(rand() % (int)(2.0f * DUST_RANGE * 10.0f)) / 10.0f - DUST_RANGE);
            while (d.x < 0) d.x += gw;
            while (d.x > gw) d.x -= gw;
            d.y = terrainYAt(tl, d.x, 480.0f) - ((float)(rand() % 350) / 10.0f + 3.0f);
            d.vy = (float)(rand() % 401) / 100.0f - 2.0f;
            if (nearF > 0.6f) d.vy -= nearF * 2.5f;
            d.life = DUST_LIFE * (0.5f + (float)(rand() % 50) / 100.0f);
        }
    }
}

static void shadedHLine(Renderer &r, float x0, float x1, float y, int brightness)
{
    int a = (int)roundf(x0), b = (int)roundf(x1);
    if (a > b) { int t = a; a = b; b = t; }
    for (int x = a; x <= b; x++) r.pixelShade((float)x, y, brightness);
}

float Game::landingProximity() const
{
    float w = terrain.getWidth();
    const std::vector<TerrainLine> &tl = terrain.getLines();
    float best = 1e9f;
    for (int i = 0; i < (int)tl.size(); i++) {
        if (tl[i].labelX < 0) continue;
        float d = fabsf(ship.posX - tl[i].x1);
        if (d > w * 0.5f) d = w - d;
        if (d < best) best = d;
    }
    if (best >= 1e9f) return 0.0f;
    return 1.0f - fminf(best / DUST_NEAR_RANGE, 1.0f);
}

void Game::bakeBg()
{
    if (!worldBg.ready()) return;
    float maxY = 0.0f;
    const std::vector<TerrainLine> &ls = terrain.getLines();
    for (int i = 0; i < (int)ls.size(); i++)
        if (ls[i].y2 > maxY) maxY = ls[i].y2;
    if (worldBg.height() < (int)(maxY + 2.0f)) return;
    LayerPainter p;
    p.begin(worldBg.data(), worldBg.width(), worldBg.height());
    p.clear();
    terrain.draw(p, 0.0f, 0.0f, 1.0f, 0, false, false);
    bgBakedRev = terrain.revision();
    bgBaked = true;
}

void Game::drawWind(Renderer &r)
{
    if (!windEnabled) return;

    float altFactor = 1.0f - fminf(ship.altitude / WIND_ALT_MAX, 1.0f);

    if (!windStreaks.empty()) {
        int visible = (int)(windStreaks.size() * altFactor);
        if (visible < WIND_STREAK_MIN_VISIBLE) visible = WIND_STREAK_MIN_VISIBLE;
        if (visible > (int)windStreaks.size()) visible = (int)windStreaks.size();
        if (zoomedIn && visible > 8) visible = 8;
        float dir = (float)windDir;
        for (int i = 0; i < visible; i++) {
            float sx = windStreaks[i].x * viewScale + viewX;
            float sy = windStreaks[i].y * viewScale + viewY;
            if (sy < -30.0f || sy > SCREEN_H + 30.0f) continue;

            float base = (WIND_STREAK_MIN +
                          (WIND_STREAK_MAX - WIND_STREAK_MIN) * windStrength) * viewScale;
            float lenA = base * windStreaks[i].f1;
            float lenB = base * windStreaks[i].f2;
            shadedHLine(r, sx, sx + dir * lenA, sy, 180);
            float forkIn = clampf((altFactor - WIND_FORK_ALT) / (1.0f - WIND_FORK_ALT),
                                  0.0f, 1.0f);
            if (forkIn > 0.0f) {
                shadedHLine(r, sx, sx + dir * lenB, sy + 1, (int)(180.0f * forkIn));
            }
            float lenW = (forkIn > 0.0f) ? fmaxf(lenA, lenB) : lenA;
            shadedHLine(r, sx - dir * lenW * 0.5f, sx - dir * lenW * 0.5f + dir * lenW * 0.45f, sy, 90);
            shadedHLine(r, sx - dir * lenW * 0.9f, sx - dir * lenW * 0.9f + dir * lenW * 0.3f, sy, 45);
        }
    }

    float nearF = landingProximity();

    for (int i = 0; i < (int)dust.size(); i++) {
        float sx = dust[i].x * viewScale + viewX;
        float sy = dust[i].y * viewScale + viewY;
        if (sy < -20.0f || sy > SCREEN_H + 20.0f) continue;
        if (sx < -20.0f || sx > SCREEN_W + 20.0f) continue;

        if (nearF < 0.15f) {
            if ((i & 1) == 0) continue;
            r.pixelShade(sx, sy, 45);
        } else {
            float flick = 0.8f + 0.2f * sinf(windPhase * 2.0f + i * 1.7f);
            float t = (nearF - 0.15f) / 0.85f;
            int b = (int)(45.0f + 205.0f * t * t * windStrength * flick);
            if (b > 250) b = 250;
            r.pixelShade(sx, sy, b);
            if (nearF > 0.7f && b > 120) {
                int hb = b / 2;
                r.pixelShade(sx - 1.0f, sy, hb);
                r.pixelShade(sx + 1.0f, sy, hb);
                r.pixelShade(sx, sy - 1.0f, hb);
                r.pixelShade(sx, sy + 1.0f, hb);
            }
        }
    }
}

void Game::setZoom(bool zoom, float zm)
{
    if (zoom) {
        viewScale = SCREEN_H / 700.0f * zm;
        zoomedIn = true;
        viewX = -ship.posX * viewScale + SCREEN_W / 2.0f;
        // Lower zoom → ship sits lower on screen (shows more sky overhead).
        float shipFrac = 0.25f + (1.0f - zm / 5.0f) * 0.55f;
        viewY = -ship.posY * viewScale + SCREEN_H * shipFrac;
        // Scale ship so it reads the same size as the 5x zoom reference.
        ship.scale = 0.48f * 5.0f / zm;
        if (ship.scale > 1.5f) ship.scale = 1.5f;
        if (ship.scale < 0.48f) ship.scale = 0.48f;
    } else {
        viewScale = SCREEN_H / 700.0f;
        zoomedIn = false;
        ship.scale = 1.5f;
        viewX = 0;
        viewY = 0;
    }
}

void Game::updateView()
{
    float margintop = SCREEN_H * 0.2f;
    float marginbottom = SCREEN_H * 0.3f;
    float marginx = SCREEN_W * 0.2f;

    bool inZone = tanker.active && !tanker.done &&
                  fabsf(ship.posX - tanker.bodyX) < TANKER_DOCK_ZONE_X &&
                  fabsf(ship.posY - tanker.portY) < TANKER_DOCK_ZONE_Y;
    // Hysteresis: enter the dock zoom inside the zone, leave it only once well
    // outside, so the ship hovering at the boundary doesn't flicker the view.
    bool nearZone = tanker.active && !tanker.done &&
                    fabsf(ship.posX - tanker.bodyX) < TANKER_DOCK_ZONE_X + 30.0f &&
                    fabsf(ship.posY - tanker.portY) < TANKER_DOCK_ZONE_Y + 20.0f;
    bool tankerZone = inZone || (tankerZooming && nearZone);
    tankerZooming = tankerZone;

    // Aerial docking: the viewport goes to a macro zoom centered on the SHIP
    // (not the midpoint) so the player never loses sight of their module; the
    // PiP window shows the magnified probe/drogue contact point.
    if (tankerZone) {
        if (!zoomedIn) setZoom(true);
        viewX = SCREEN_W * 0.5f - ship.posX * viewScale;
        viewY = SCREEN_H * 0.5f - ship.posY * viewScale;
        return;
    }

    // While the wormhole vortex owns the ship the zoom transitions are frozen
    // (the ship shrinks toward the core; setZoom would reset its scale). Same
    // during the respawn fade-in: the materialization ramps ship.scale and
    // must not be overridden by a zoom transition.
    //
    // Approach altitude for the zoom thresholds: ship.altitude is measured
    // from ship.bottom = posY + 14*ship.scale, and ship.scale itself changes
    // with the zoom (1.5 normal / 0.48 at 5x). Using ship.altitude directly
    // makes the altitude jump ~14u the instant the zoom flips, which
    // oscillated the zoom around the threshold. Measure from the ship's
    // center (posY), which is zoom-independent.
    float approachAlt = 9999.0f;
    const std::vector<TerrainLine> &tls = terrain.getLines();
    for (int i = 0; i < (int)tls.size(); i++) {
        if (ship.posX >= tls[i].x1 && ship.posX <= tls[i].x2) {
            float a = tls[i].y1 - ship.posY - 14.0f;
            if (a < approachAlt) approachAlt = a;
        }
    }
    if (approachAlt > 9998.0f) approachAlt = ship.altitude;

    if (!wormhole.captured() && warpInT <= 0.0f) {
        if (moonHasRings(level)) {
            // Rings zoom: band-proximity OR final-approach altitude.
            // Entering the debris band -> zoom-in to weave through the rocks.
            // After exiting the band, if the ship is still high -> zoom-out.
            // Near the ground, the normal altitude threshold kicks in for
            // the final landing approach zoom (same as all other moons).
            float bandCY = rings.centerBandY(terrain, ship.posX);
            float bandTop = bandCY - RING_Y_JITTER - 30.0f;
            float bandBot = bandCY + RING_Y_JITTER + 30.0f;
            float bandTopOut = bandCY - RING_Y_JITTER - 60.0f;
            float bandBotOut = bandCY + RING_Y_JITTER + 60.0f;

            bool inBand = ship.posY >= bandTop && ship.posY <= bandBot;
            bool nearBand = ship.posY >= bandTopOut && ship.posY <= bandBotOut;
            bool lowAlt = approachAlt < APPROACH_ALT;

            if (!zoomedIn && (inBand || lowAlt)) {
                setZoom(true, 2.0f);
            } else if (zoomedIn && !nearBand && approachAlt > APPROACH_EXIT_ALT) {
                setZoom(false);
            }
        } else {
            float zi = APPROACH_ALT;
            float zo = APPROACH_EXIT_ALT;
            if (!zoomedIn && approachAlt < zi) {
                setZoom(true);
            } else if (zoomedIn && approachAlt > zo) {
                setZoom(false);
            }
        }
    }

    // Camera tracking: in approach zoom the camera follows the ship centered
    // horizontally and at ~1/3 from the top vertically, so the terrain below
    // (the landing zone) fills the bottom 2/3 of the screen.  In normal view
    // the margins keep the ship on-screen.
    if (zoomedIn) {
        viewX = -ship.posX * viewScale + SCREEN_W * 0.5f;
        viewY = -ship.posY * viewScale + SCREEN_H * APPROACH_CAM_FRAC;
    } else {
        float sx = ship.posX * viewScale + viewX;
        if (sx < marginx) viewX = -ship.posX * viewScale + marginx;
        else if (sx > SCREEN_W - marginx) viewX = -ship.posX * viewScale + SCREEN_W - marginx;

        float sy = ship.posY * viewScale + viewY;
        if (sy < margintop) viewY = -ship.posY * viewScale + margintop;
        else if (sy > SCREEN_H - marginbottom) viewY = -ship.posY * viewScale + SCREEN_H - marginbottom;
    }
}

// Culling tests against the world-space rectangle currently visible: a world
// point expanded by a margin (the effect's reach) and a full-width horizontal
// band checked vertically. viewX/viewY/viewScale hold the CURRENT view; in
// zoom the visible slice is only ~187 world units wide, so effects whose
// anchor is far outside it are skipped entirely (their per-frame draw cost is
// the expensive part — e.g. the wormhole runs ~900 powf + thousands of
// pixelShade calls every frame even when fully off screen).
bool Game::effectVisible(float wx, float wy, float margin) const
{
    float x0 = -viewX / viewScale;
    float x1 = (SCREEN_W - viewX) / viewScale;
    float y0 = -viewY / viewScale;
    float y1 = (SCREEN_H - viewY) / viewScale;
    return wx + margin >= x0 && wx - margin <= x1 &&
           wy + margin >= y0 && wy - margin <= y1;
}

bool Game::xInView(float wx, float margin) const
{
    float x0 = -viewX / viewScale;
    float x1 = (SCREEN_W - viewX) / viewScale;
    return wx + margin >= x0 && wx - margin <= x1;
}

bool Game::bandVisible(float wy, float margin) const
{
    float y0 = -viewY / viewScale;
    float y1 = (SCREEN_H - viewY) / viewScale;
    return wy + margin >= y0 && wy - margin <= y1;
}

bool Game::atmosphereInView() const
{
    if (!atmosphere.active()) return false;
    // Fog bands span the whole world width, so only the vertical extent
    // matters (ellipse arc + band half + vertical drift).
    const float m = FOG_BAND_HALF * 2.0f + FOG_DRIFT_A + FOG_CURVE_A;
    for (int i = 0; i < atmosphere.bandCount(); i++)
        if (bandVisible(atmosphere.bandCenter(i), m)) return true;
    return false;
}

void Game::checkCollisions()
{
    // A rock from the debris rings shatters the ship if it hits it mid-flight.
    if (rings.hitsShip(terrain, ship.posX, ship.posY, RING_SHIP_RADIUS)) {
        ringHit = true;
        ship.crash();
        int lost = 200 + (rand() % 200);
        fuel -= lost;
        ship.fuel -= lost;
        if (ship.fuel < 0) ship.fuel = 0;
        if (fuel < 0) fuel = 0;
        score += 5;
        state = STATE_CRASHED;
        resetTimer = CRASH_RESET_DELAY;
        return;
    }

    // Aerial tanker: touching the mothership hull destroys BOTH ships.
    if (tanker.hitsHull(ship.posX, ship.posY)) {
        tankerCrash = true;
        tanker.destroy();
        ship.crash(true);
        int lost = 200 + (rand() % 200);
        fuel -= lost;
        ship.fuel -= lost;
        if (ship.fuel < 0) ship.fuel = 0;
        if (fuel < 0) fuel = 0;
        score += 5;
        state = STATE_CRASHED;
        resetTimer = TANKER_CRASH_DURATION;
        return;
    }

    // Probe-and-drogue: docking in flight plugs the probe into a drogue basket
    // and fuel flows incrementally while the connection holds.
    if (tanker.targeted()) {
        if (tanker.checkDock(ship)) {
            tanker.beginDock(ship.velX, ship.velY);
            return;
        }
    }

    int result = terrain.checkLanding(
        ship.left, ship.right, ship.bottom,
        ship.rotation, ship.velY, ship.velX
    );

    // Touching lava burns the ship on any collision (good landing or hard
    // crash), so a touchdown over lava always shows the burnt-ship ending.
    if (result != 0 && volcanoes.landOnLava(ship.left, ship.right)) {
        lavaBurn = true;
        ship.land();
        state = STATE_CRASHED;
        resetTimer = CRASH_RESET_DELAY;
        return;
    }

    // Touching the ground while the twister is dragging the ship smashes it
    // against the ground near the vortex base.
    if (result != 0 && twister.captured()) {
        twisterCrash = true;
        terrain.setCrater(ship.posX, CRATER_HALF_W);
        ship.crash();
        int lost = 200 + (rand() % 200);
        fuel -= lost;
        ship.fuel -= lost;
        if (ship.fuel < 0) ship.fuel = 0;
        if (fuel < 0) fuel = 0;
        score += 5;
        state = STATE_CRASHED;
        resetTimer = CRASH_RESET_DELAY;
        return;
    }

    if (result == 2) {
        float mult = 1.0f;
        for (int i = 0; i < (int)terrain.getLines().size(); i++) {
            const TerrainLine &l = terrain.getLines()[i];
            if (ship.posX >= l.x1 && ship.posX <= l.x2 && l.multiplier > 1) {
                mult = (float)l.multiplier;
                break;
            }
        }

        ship.land();
        // Decide the landing outcome ONCE at touchdown: velY keeps evolving
        // while the landing message is shown (gravity, parachute braking), so
        // the bonus and the on-screen verdict must share this fixed value.
        landPerfect = ship.velY < LAND_PERFECT_VY;
        if (landPerfect) {
            score += (int)(50 * mult);
            // The +50 fuel is NOT applied here: the player is focused on the
            // landing spot, not the HUD. It lands at the next level start,
            // where the reward is clearly visible in the FUEL counter.
            landFuelBonus = 50;
        } else {
            score += (int)(15 * mult);
            hullIntegrity -= 15.0f;
            if (hullIntegrity < 0.0f) hullIntegrity = 0.0f;
        }
        if (!chuteAvailable && terrain.onChuteSpot(ship.posX)) chuteAvailable = true;
        state = STATE_LANDED;
        resetTimer = CRASH_RESET_DELAY;
    } else if (result == 1) {
        if (terrain.isRupturedAt(ship.posX)) {
            quakeCrash = true;
        }
        int lost = 200 + (rand() % 200);
        terrain.setCrater(ship.posX, CRATER_HALF_W);
        ship.crash();
        fuel -= lost;
        ship.fuel -= lost;
        if (ship.fuel < 0) ship.fuel = 0;
        if (fuel < 0) fuel = 0;
        score += 5;
        state = STATE_CRASHED;
        resetTimer = CRASH_RESET_DELAY;
    }
}

void Game::update()
{
    float dt = GAME_DT;
    updateWind(dt);
    if (fuelMaxTimer > 0.0f) fuelMaxTimer -= dt;
    if (chuteTooLowTimer > 0.0f) chuteTooLowTimer -= dt;
    if (recycledTimer > 0.0f) {
        recycledTimer -= dt;
        if (recycledTimer < 0.0f) recycledTimer = 0.0f;
    }
    ship.windStrength = windEnabled ? windStrength : 0.0f;
    ship.windDir = windDir;
    ship.gravity = GRAVITY * moonGravity(level);
    if (state != STATE_WAITING) storm.update(dt, terrain);
    if (state != STATE_WAITING) geysers.update(dt);
    if (state != STATE_WAITING) volcanoes.update(dt);
    // Titan fog: t_ only drives the band drift (the drag/downdraft/hidesShip
    // hooks run directly in Game), so there is nothing to keep alive while the
    // bands are fully out of view.
    if (state != STATE_WAITING && atmosphereInView()) atmosphere.update(dt);
    if (state != STATE_WAITING) rings.update(dt);
    if (state != STATE_WAITING) twister.update(dt);
    // Wormhole: its update only advances visual state (spin/phase/particles).
    // Freeze it while the hole is off screen AND cannot reach the ship, but
    // keep driving it during the pull/vortex/teleport sequences (Game waits on
    // the swallow/dying phases to advance before jumping moons).
    if (state != STATE_WAITING && wormhole.active()) {
        float dx = wormhole.coreX() - ship.posX;
        float dy = wormhole.coreY() - ship.posY;
        bool nearShip = dx * dx + dy * dy < WORMHOLE_GRAB_R * WORMHOLE_GRAB_R;
        if (effectVisible(wormhole.coreX(), wormhole.coreY(), WORMHOLE_OUTER_R) ||
            nearShip || wormhole.captured() || wormhole.swallowed())
            wormhole.update(dt);
    }
    if (state != STATE_WAITING) acidrain.update(dt);
    if (state != STATE_WAITING) quake.update(dt, terrain, ship, state == STATE_PLAYING);

    // The quake just buckled the ground: re-run the lava flows so the ribbons
    // follow the new surface instead of the pre-quake terrain.
    if (state != STATE_WAITING && quake.justStruck()) volcanoes.rebuild(terrain);

    if (input.startPressed && demo) {
        demo = false;
        newGame();
        input.startPressed = false;
        return;
    }

    if (state == STATE_WAITING) {
        ship.update();
        ship.altitude = terrain.getLines()[0].y1 - ship.bottom;
        if (input.startPressed) {
            newGame();
            input.startPressed = false;
        } else {
            demoTimer -= dt;
            if (demoTimer <= 0) startDemo();
        }
        return;
    }

    if (state == STATE_PLAYING) {
        if (wormhole.swallowed()) {
            // The swallow flash + hole fade play out first (the ship is hidden
            // while swallowed), then the ship fades in on another moon (or the
            // demo returns to the title). Physics stays frozen meanwhile.
            if (!wormhole.active()) {
                // Full sequence even in the attract demo: the hole swallows the
                // ship and it fades back in on another moon (wormholeJump), so
                // the demo shows the whole teleport; the autopilot then keeps
                // flying on the new moon until the level ends (back to title).
                wormholeJump();
                if (demo) setupDemoTarget();
            }
            return;
        }

        if (introTimer > 0) {
            ship.left = ship.posX - 10.0f * ship.scale;
            ship.right = ship.posX + 10.0f * ship.scale;
            ship.bottom = ship.posY + 14.0f * ship.scale;
            ship.top = ship.posY - 5.0f * ship.scale;
            float minAlt = 9999;
            for (int i = 0; i < (int)terrain.getLines().size(); i++) {
                const TerrainLine &l = terrain.getLines()[i];
                if (ship.posX >= l.x1 && ship.posX <= l.x2) {
                    float alt = l.y1 - ship.bottom;
                    if (alt < minAlt) minAlt = alt;
                }
            }
            if (minAlt >= 9999) minAlt = 300.0f;
            ship.altitude = minAlt;
            introTimer -= dt;
            if (introTimer < 0) introTimer = 0;
            return;
        }

        // One-shot parachute: Start button deploys it once; opening too low is
        // ignored and flashes a warning (the canopy can't open in time). It is
        // consumed on deploy and only recovered by landing on the marked pad.
        if (input.startPressed && !ship.chute) {
            if (chuteAvailable) {
                if (ship.altitude > PARACHUTE_MIN_ALT) {
                    ship.chute = true;
                    ship.chuteOpen = 0.0f;
                    chuteAvailable = false;
                    input.startPressed = false;
                } else {
                    chuteTooLowTimer = 1.2f;
                    input.startPressed = false;
                }
            }
        }

        if (tanker.docked) {
            if (demo) runDemoAI();

            // Docking mini-game: the joystick X axis micro-adjusts the probe
            // horizontally inside the drogue. Neutral stick (angle 0) lets the
            // centering spring keep the probe aligned.
            ship.velX = sinf(input.angle) * 0.8f;
            ship.velY = 0.0f;
            // Firing the engine (Z button held) breaks the connection: you get
            // only the fuel accumulated so far. The tanker stays on station.
            if (!demo && input.thrust > 0.0f) {
                tanker.breakAway(ship);
            } else if (tanker.update(dt, ship)) {
                fuelMaxTimer = 1.5f;
                fuel = ship.fuel;
                if (demo) {
                    const std::vector<TerrainLine> &tl2 = terrain.getLines();
                    float bestD = 1e9f;
                    int bestI = -1;
                    for (int i = 0; i < (int)tl2.size(); i++) {
                        if (tl2[i].labelX >= 0) {
                            float d = fabsf(tl2[i].labelX - ship.posX);
                            if (d < bestD) { bestD = d; bestI = i; }
                        }
                    }
                    if (bestI >= 0) {
                        demoTargetX = tl2[bestI].labelX;
                        demoTargetY = terrain.yAt(tl2[bestI].labelX, 500.0f);
                        demoHoldAltitude = true;
                    }
                }
            } else {
                fuel = ship.fuel;
            }
            ship.left = ship.posX - 10.0f * ship.scale;
            ship.right = ship.posX + 10.0f * ship.scale;
            ship.bottom = ship.posY + 14.0f * ship.scale;
            ship.top = ship.posY - 5.0f * ship.scale;
            updateView();
            return;
        }

        if (demo) runDemoAI();

        float deg = input.angle * 180.0f / PI;
        if (ship.chute) {
            // Dirigible glide: the canopy auto-levels the ship and the stick
            // steers laterally instead of rotating. The engine still works, so
            // a short burst can flare the touchdown into a perfect landing. A
            // lightning hit scrambles the steering for the loss duration.
            ship.setTargetRotation(0.0f);
            if (stormHitTimer > 0.0f) {
                stormHitTimer -= dt;
                if (stormHitTimer < 0.0f) stormHitTimer = 0.0f;
                float sdeg = ((float)(rand() % 2001) / 1000.0f - 1.0f) * 0.5f * PI;
                ship.velX += sinf(sdeg) * PARACHUTE_STEER * ship.chuteOpen;
                ship.setThrust(0.0f);
            } else {
                ship.velX += sinf(input.angle) * PARACHUTE_STEER * ship.chuteOpen;
                ship.setThrust(input.thrust);
                if (storm.strikes(ship.posX, ship.posY, STORM_HIT_RADIUS)) {
                    float lost = STORM_HIT_FUEL;
                    fuel -= lost;
                    ship.fuel -= lost;
                    if (fuel < 0) fuel = 0;
                    if (ship.fuel < 0) ship.fuel = 0;
                    stormHitTimer = STORM_CONTROL_LOSS;
                }
            }
        } else if (warpInT <= 0.0f) {
            if (stormHitTimer > 0.0f) {
                stormHitTimer -= dt;
                if (stormHitTimer < 0.0f) stormHitTimer = 0.0f;
                deg += ((float)(rand() % 2001) / 1000.0f - 1.0f) * 0.5f * 180.0f / PI;
                ship.setTargetRotation(deg);
                ship.setThrust(0.0f);
            } else {
                ship.setTargetRotation(deg);
                ship.setThrust(input.thrust);
                if (storm.strikes(ship.posX, ship.posY, STORM_HIT_RADIUS)) {
                    float lost = STORM_HIT_FUEL;
                    fuel -= lost;
                    ship.fuel -= lost;
                    if (fuel < 0) fuel = 0;
                    if (ship.fuel < 0) ship.fuel = 0;
                    stormHitTimer = STORM_CONTROL_LOSS;
                }
            }
        }
        ship.update();
        if (demo && ship.fuel <= 0) {
            ship.fuel = FUEL_MAX;
            fuel = FUEL_MAX;
        }
        if (geysers.inPlume(ship.posX, ship.posY))
            ship.velY -= GEYSER_PUSH * (ship.gravity / GRAVITY);

        // Acid rain on Europa: rain inside a drifting cell corrodes the ship;
        // at 100% the acid has eaten through the hull and the ship is lost.
        // Hull damage from acid is cumulative across the whole game.
        if (acidrain.active()) {
            if (acidrain.inRain(ship.posX, ship.posY)) {
                acidrain.corrode();
                hullIntegrity -= 0.5f * dt;
            } else {
                acidrain.dry();
            }
            if (hullIntegrity <= 0.0f) hullIntegrity = 0.0f;
            if (acidrain.meterGet() >= 100.0f || hullIntegrity <= 0.0f) {
                acidBurn = true;
                ship.dissolve();
                int lost = 200 + (rand() % 200);
                fuel -= lost;
                ship.fuel -= lost;
                if (ship.fuel < 0) ship.fuel = 0;
                if (fuel < 0) fuel = 0;
                score += 5;
                state = STATE_CRASHED;
                resetTimer = CRASH_RESET_DELAY;
                return;
            }
        }

        if (twister.active()) {
            twister.apply(ship, terrain, input.angle * 180.0f / PI);
        }

        // Sky wormhole: OUTSIDE half the action radius it pulls radially
        // toward the nucleus (the ship stays controllable and escapes by
        // thrusting away); INSIDE it the ship is captured and scripted into a
        // shrinking vortex that swallows it. Collisions stay on in the pull
        // zone; they are skipped while the vortex owns the ship.
        bool wormholePulled = false;
        if (wormhole.active()) wormholePulled = wormhole.apply(ship);
        if (wormhole.captured()) ship.setThrust(0.0f); // no control while trapped

        if (atmosphere.active()) {
            ship.velX *= ATMOS_DRAG;
            ship.velY += ATMOS_DOWN;
        }

        if (tanker.active && !tanker.docked && !tanker.done) tanker.update(dt, ship);

        if (ship.posX > terrain.getWidth() + 10)
            ship.posX = -10;
        else if (ship.posX < -10)
            ship.posX = terrain.getWidth() + 10;

        if (ship.posY < -50) {
            ship.reset(110, 150);
            ship.velX = 2;
            setZoom(false);
        }

        float minAlt = 9999;
        for (int i = 0; i < (int)terrain.getLines().size(); i++) {
            const TerrainLine &l = terrain.getLines()[i];
            if (ship.posX >= l.x1 && ship.posX <= l.x2) {
                float alt = l.y1 - ship.bottom;
                if (alt < minAlt) minAlt = alt;
            }
        }
        ship.altitude = minAlt;

        updateView();

        // Wormhole respawn: the level already plays (no intro announcement)
        // while the ship materializes, growing from nothing to its normal size
        // over WORMHOLE_WARP_IN_T. Runs after updateView so the zoom stays
        // frozen on the very frame the ramp finishes (setZoom would reset
        // ship.scale), and draw always sees the ramped scale.
        if (warpInT > 0.0f) {
            warpInT -= dt;
            if (warpInT < 0.0f) warpInT = 0.0f;
            float wp = 1.0f - warpInT / WORMHOLE_WARP_IN_T;
            if (wp < 0.0f) wp = 0.0f;
            ship.scale = 1.5f * wp;
            float decay = 1.0f - wp;
            if (decay < 0.0f) decay = 0.0f;
            ship.rotation = 1080.0f * decay * decay;
        }

        ship.left = ship.posX - 10.0f * ship.scale;
        ship.right = ship.posX + 10.0f * ship.scale;
        ship.bottom = ship.posY + 14.0f * ship.scale;
        if (!wormholePulled) checkCollisions();
        return;
    }

    if (state == STATE_LANDED || state == STATE_CRASHED) {
        ship.update();
        resetTimer -= dt;
        if (resetTimer <= 0) {
            if (demo) {
                printf("[demo] level end state=%s fuel=%.1f\n",
                              state == STATE_LANDED ? "LANDED" : "CRASHED", ship.fuel);
                endDemoToTitle();
                landFuelBonus = 0;
            } else if (state == STATE_LANDED) {
                // Perfect-landing fuel bonus is granted here, at the level
                // transition: the player is relaxed and sees 300 -> 350 in
                // the FUEL counter of the next level.
                if (landFuelBonus > 0) {
                    ship.fuel += (float)landFuelBonus;
                    if (ship.fuel > FUEL_MAX) ship.fuel = FUEL_MAX;
                    fuel = ship.fuel;
                    landFuelBonus = 0;
                }
                if (ship.fuel <= 0 || hullIntegrity <= 0.0f) {
                    endGame();
                } else {
                    nextLevel();
                }
            } else if (ship.fuel <= 0) {
                endGame();
            } else {
                restartLevel();
            }
        }
        return;
    }

    if (state == STATE_GAMEOVER) {
        ship.update();
        resetTimer -= dt;
        if (resetTimer <= 0) {
            state = STATE_WAITING;
            demo = false;
            demoTimer = DEMO_START_DELAY;
            setZoom(false);
            wormhole.disable();
            setupTitleShip();
        }
    }
}

void Game::draw(Renderer &r)
{
    r.clear();

    if (state != STATE_WAITING) storm.drawSky(r, viewX, viewY, viewScale);

    int warnY = 62;

    if (state == STATE_WAITING) {
        for (int i = 0; i < TITLE_STAR_COUNT; i++) {
            r.rect((float)titleStars[i][0], (float)titleStars[i][1], 1.0f, 1.0f);
        }
        terrain.draw(r, viewX, viewY, viewScale, ship.counter, false);
        ship.draw(r, viewX, viewY, viewScale);

        r.textScaled(70, 20, "LUNAR LANDER++", 2.0f, 255);

        auto centerText = [&r](float y, const char *s) {
            r.text((SCREEN_W - (int)strlen(s) * 6) / 2.0f, y, s);
        };
        centerText(40, "Copyright Alex Urzola 2026/Opencode");

        if ((ship.counter % 50) < 30) r.text(170, 120, "PRESS BUTTON TO PLAY");

        auto SX = [](float x) { return x * 1.2f + 26.0f; };
        auto SY = [](float y) { return y * 1.2f + 56.0f; };

        // Descent stage fill + wireframe
        {
            float xs[4] = {SX(40), SX(60), SX(70), SX(30)};
            float ys[4] = {SY(55), SY(55), SY(80), SY(80)};
            r.fillPolygon(xs, ys, 4, 100);
        }
        r.line(SX(40), SY(55), SX(60), SY(55));
        r.line(SX(40), SY(55), SX(30), SY(80));
        r.line(SX(60), SY(55), SX(70), SY(80));
        r.line(SX(30), SY(80), SX(70), SY(80));

        // Left leg: V-strut twin members + cross-brace zigzag + footpad.
        {
            float xs[4] = {SX(30), SX(33), SX(7), SX(4)};
            float ys[4] = {SY(80), SY(79), SY(95), SY(95)};
            r.fillPolygon(xs, ys, 4, 110);
        }
        r.line(SX(30), SY(80), SX(4), SY(95));
        r.line(SX(33), SY(79), SX(7), SY(95));
        for (int k = 0; k < 5; k++) {
            float ta = (float)k / 5.0f, tb = ((float)k + 0.5f) / 5.0f;
            r.line(SX(30 + (4 - 30) * ta), SY(80 + (95 - 80) * ta),
                   SX(33 + (7 - 33) * tb), SY(79 + (95 - 79) * tb));
        }
        r.rectShade(SX(1), SY(93), 7.2f, 3.0f, 80);
        r.line(SX(1), SY(93), SX(8), SY(93));
        r.line(SX(1), SY(93), SX(1), SY(96));
        r.line(SX(8), SY(93), SX(8), SY(96));
        r.line(SX(1), SY(96), SX(8), SY(96));

        // Right leg: mirrored V-strut.
        {
            float xs[4] = {SX(67), SX(70), SX(96), SX(93)};
            float ys[4] = {SY(79), SY(80), SY(95), SY(95)};
            r.fillPolygon(xs, ys, 4, 110);
        }
        r.line(SX(70), SY(80), SX(96), SY(95));
        r.line(SX(67), SY(79), SX(93), SY(95));
        for (int k = 0; k < 5; k++) {
            float ta = (float)k / 5.0f, tb = ((float)k + 0.5f) / 5.0f;
            r.line(SX(70 + (96 - 70) * ta), SY(80 + (95 - 80) * ta),
                   SX(67 + (93 - 67) * tb), SY(79 + (95 - 79) * tb));
        }
        r.rectShade(SX(91), SY(93), 7.2f, 3.0f, 80);
        r.line(SX(91), SY(93), SX(98), SY(93));
        r.line(SX(91), SY(93), SX(91), SY(96));
        r.line(SX(98), SY(93), SX(98), SY(96));
        r.line(SX(91), SY(96), SX(98), SY(96));

        // Center leg: twin parallel struts + cross-braces + footpad.
        {
            float xs[4] = {SX(49), SX(51), SX(51), SX(49)};
            float ys[4] = {SY(80), SY(80), SY(95), SY(95)};
            r.fillPolygon(xs, ys, 4, 110);
        }
        r.line(SX(49), SY(80), SX(49), SY(95));
        r.line(SX(51), SY(80), SX(51), SY(95));
        for (int k = 0; k < 4; k++) {
            float yk = SY(84 + k * 3.0f);
            r.line(SX(49), yk, SX(51), yk);
        }
        r.rectShade(SX(46), SY(93), 7.2f, 3.0f, 80);
        r.line(SX(46), SY(93), SX(53), SY(93));
        r.line(SX(46), SY(93), SX(46), SY(96));
        r.line(SX(53), SY(93), SX(53), SY(96));
        r.line(SX(46), SY(96), SX(53), SY(96));

        r.rect(SX(48), SY(55), 4.8f, 30.0f);
        r.line(SX(48), SY(57), SX(52), SY(57));
        r.line(SX(48), SY(61), SX(52), SY(61));
        r.line(SX(48), SY(65), SX(52), SY(65));
        r.line(SX(48), SY(69), SX(52), SY(69));
        r.line(SX(48), SY(73), SX(52), SY(73));
        r.line(SX(48), SY(77), SX(52), SY(77));

        for (int y = 56; y < 79; y += 2) {
            for (float x = 40 + (y - 56) * 0.5f; x < 60 - (y - 56) * 0.5f; x += 3) {
                r.pixel(SX(x), SY((float)y));
            }
        }
        for (int y = 56; y < 79; y += 1) {
            r.pixel(SX(38 - (y - 56) * 0.3f), SY((float)y));
            r.pixel(SX(37 - (y - 56) * 0.3f), SY((float)y));
            r.pixel(SX(62 + (y - 56) * 0.3f), SY((float)y));
            r.pixel(SX(63 + (y - 56) * 0.3f), SY((float)y));
        }

        // Ascent stage fill + wireframe
        {
            float xs[6] = {SX(35), SX(35), SX(45), SX(55), SX(65), SX(65)};
            float ys[6] = {SY(55), SY(30), SY(15), SY(15), SY(30), SY(55)};
            r.fillPolygon(xs, ys, 6, 140);
        }
        r.line(SX(35), SY(55), SX(35), SY(30));
        r.line(SX(65), SY(55), SX(65), SY(30));
        r.line(SX(35), SY(30), SX(45), SY(15));
        r.line(SX(65), SY(30), SX(55), SY(15));
        r.line(SX(45), SY(15), SX(55), SY(15));

        {
            float xs[4] = {SX(43), SX(57), SX(60), SX(40)};
            float ys[4] = {SY(30), SY(30), SY(50), SY(50)};
            r.fillPolygon(xs, ys, 4, 100);
        }
        r.line(SX(43), SY(30), SX(57), SY(30));
        r.line(SX(43), SY(30), SX(40), SY(50));
        r.line(SX(57), SY(30), SX(60), SY(50));
        r.line(SX(40), SY(50), SX(60), SY(50));
        r.rectShade(SX(48), SY(35), 4.8f, 12.0f, 160);
        r.rectShade(SX(48), SY(35), 4.8f, 12.0f, 160);
        r.line(SX(48), SY(35), SX(52), SY(35));
        r.line(SX(48), SY(35), SX(48), SY(47));
        r.line(SX(52), SY(35), SX(52), SY(47));
        r.line(SX(48), SY(47), SX(52), SY(47));

        r.rectShade(SX(28), SY(38), 8.4f, 9.6f, 120);
        r.line(SX(28), SY(38), SX(36), SY(38));
        r.line(SX(28), SY(38), SX(28), SY(47));
        r.line(SX(36), SY(38), SX(36), SY(47));
        r.line(SX(28), SY(47), SX(36), SY(47));
        r.line(SX(28), SY(38), SX(26), SY(40));
        r.rectShade(SX(65), SY(38), 8.4f, 9.6f, 120);
        r.line(SX(65), SY(38), SX(73), SY(38));
        r.line(SX(65), SY(38), SX(65), SY(47));
        r.line(SX(73), SY(38), SX(73), SY(47));
        r.line(SX(65), SY(47), SX(73), SY(47));
        r.line(SX(72), SY(38), SX(74), SY(40));

        for (int y = 16; y < 30; y += 2) {
            r.line(SX(45 - (y - 15) * 0.1f), SY((float)y), SX(46 - (y - 15) * 0.1f), SY((float)y));
            r.line(SX(54 + (y - 15) * 0.1f), SY((float)y), SX(55 + (y - 15) * 0.1f), SY((float)y));
        }
        for (int y = 30; y < 50; y += 1) {
            if (y % 3 == 0) {
                r.line(SX(36), SY((float)y), SX(39), SY((float)y));
                r.line(SX(61), SY((float)y), SX(64), SY((float)y));
            }
        }
        for (int y = 31; y < 49; y += 2) {
            for (int x = 40; x < 60; x += 2) {
                if ((x + y) % 4 == 0) r.pixel(SX((float)x), SY((float)y));
            }
        }

        // Tracking dish antenna (left).
        r.line(SX(32), SY(28), SX(25), SY(23));
        r.line(SX(25), SY(23), SX(23), SY(26));
        r.rect(SX(21), SY(23), 4.8f, 3.6f);
        r.circle(SX(23), SY(23), 3.6f);
        r.circle(SX(23), SY(23), 1.8f);

        // Omnidirectional antenna (top).
        r.line(SX(50), SY(15), SX(50), SY(2));
        r.circle(SX(50), SY(10), 3.6f);
        r.line(SX(47), SY(10), SX(53), SY(10));
        r.line(SX(50), SY(7), SX(50), SY(13));
        r.line(SX(50), SY(10), SX(50), SY(8));
        r.pixel(SX(50), SY(8));
        for (float a = 0; a < 6.2832f; a += 0.3491f) {
            r.pixel(SX(50 + cosf(a) * 2.5f), SY(10 + sinf(a) * 2.5f));
        }

        // RCS thrusters.
        for (int i = 0; i < 4; ++i) {
            float ox = 45 + i * 3.3f;
            float oy = 25;
            r.circle(SX(ox), SY(oy), 1.2f);
            r.pixel(SX(ox), SY(oy - 1));
            r.pixel(SX(ox), SY(oy + 1));
            r.pixel(SX(ox - 1), SY(oy));
            r.pixel(SX(ox + 1), SY(oy));
        }
        r.circle(SX(30), SY(35), 1.2f);
        r.line(SX(30), SY(34), SX(30), SY(36));
        r.circle(SX(70), SY(35), 1.2f);
        r.line(SX(70), SY(34), SX(70), SY(36));

        // Helical antenna (right).
        r.line(SX(62), SY(25), SX(68), SY(20));
        r.rect(SX(67), SY(18), 1.2f, 4.8f);
        for (int y = 18; y < 22; y += 1) {
            r.pixel(SX(68), SY((float)y));
        }

        // Ground shadows under the footpads.
        for (int x = 0; x < 10; ++x) {
            if (x % 3 == 0) r.line(SX((float)x), SY(96), SX((float)x + 1), SY(96));
        }
        for (int x = 45; x < 55; ++x) {
            if (x % 3 == 0) r.line(SX((float)x), SY(97), SX((float)x + 1), SY(97));
        }
        for (int x = 90; x < 100; ++x) {
            if (x % 3 == 0) r.line(SX((float)x), SY(96), SX((float)x + 1), SY(96));
        }

        r.text(170, 132, "STICK: ROTATION");
        r.text(170, 144, "Z: ENGINE ON/OFF");
        r.text(170, 156, "C+STICK: POWER UP/DOWN");
        r.text(170, 168, "POT: POWER LEVEL");
        r.text(170, 180, "START: PARACHUTE (1/LEVEL)");
    } else {
        float svx = viewX, svy = viewY;
        float qs = quake.shake();
        if (qs > 0) {
            int seed = ship.counter * 7349 + 1;
            float sx = ((float)((seed * 1103515245 + 12345) & 0x7fffffff) / (float)0x7fffffff - 0.5f) * qs * 2.0f;
            seed = seed * 2713 + 2;
            float sy = ((float)((seed * 1103515245 + 12345) & 0x7fffffff) / (float)0x7fffffff - 0.5f) * qs * 2.0f;
            viewX += sx;
            viewY += sy;
        }
        float baseVs = SCREEN_H / 700.0f;
        bool normalView = fabsf(viewScale - baseVs) < 0.0005f;
        if (normalView && !bgAllocFailed && !worldBg.ready()) {
            float maxY = 0.0f;
            const std::vector<TerrainLine> &ls = terrain.getLines();
            for (int i = 0; i < (int)ls.size(); i++)
                if (ls[i].y2 > maxY) maxY = ls[i].y2;
            int lh = (int)(maxY + 2.0f);
            if (lh < (int)WORLD_H) lh = (int)WORLD_H;
            bgAllocFailed = !worldBg.alloc((int)(terrain.getWidth() + 2.0f), lh);
        }
        if (worldBg.ready() && normalView) {
            if (!bgBaked || bgBakedRev != terrain.revision()) bakeBg();
            r.drawLayer(worldBg.data(), worldBg.width(), worldBg.height(),
                        viewX, viewY, viewScale);
            terrain.drawStarField(r, viewX, viewY, viewScale);
            terrain.drawLabels(r, viewX, viewY, viewScale);
        } else {
            terrain.draw(r, viewX, viewY, viewScale, ship.counter);
        }
        if (state != STATE_WAITING && atmosphereInView())
            atmosphere.drawSky(r, terrain, viewX, viewY, viewScale);
        {
            // Geysers: only draw when at least one vent (plus plume reach) is
            // in view. Vents sit on terrain, so horizontal visibility is enough.
            bool gv = false;
            for (int i = 0; i < geysers.ventCount() && !gv; i++)
                gv = xInView(geysers.ventX(i), GEYSER_SPOUT_H + 20.0f);
            if (gv) geysers.draw(r, viewX, viewY, viewScale);
        }
        if (volcanoes.countInView(viewX, viewScale) > 0)
            volcanoes.draw(r, viewX, viewY, viewScale);
        drawWind(r);
        if (rings.active() && bandVisible(RING_CY, RING_CURVE_A + RING_Y_JITTER + 16.0f))
            rings.draw(r, terrain, viewX, viewY, viewScale);
        if (effectVisible(twister.coreX(), twister.coreY(terrain), TWISTER_HEIGHT))
            twister.draw(r, terrain, viewX, viewY, viewScale, zoomedIn);
        // Hide tanker during final approach: the ship is landing, not docking.
        if (ship.altitude >= APPROACH_ALT)
            tanker.draw(r, viewX, viewY, viewScale, ship.counter, ship);
        acidrain.draw(r, terrain, viewX, viewY, viewScale);
        quake.draw(r, viewX, viewY, viewScale);
        bool fogged = (state == STATE_PLAYING) && atmosphere.hidesShip(ship.posX, ship.posY);
        if (!lavaBurn && !tankerCrash && !fogged && !wormhole.swallowed())
            ship.draw(r, viewX, viewY, viewScale);
        if (acidrain.active()) acidrain.drawSizzle(r, ship.posX, ship.posY, viewX, viewY, viewScale);

        // Refuel probe on top of the module: a thin boom with a diamond tip
        // that sticks out of the hull toward the tanker, shown during the
        // docking maneuver so the player can line it up with a drogue basket.
        // The magnified view of the contact point lives in the PiP window.
        bool tankerDockShow = tanker.active && !tanker.done &&
            fabsf(ship.posX - tanker.bodyX) < TANKER_DOCK_ZONE_X &&
            fabsf(ship.posY - tanker.portY) < TANKER_DOCK_ZONE_Y;
        if (tankerDockShow && !lavaBurn && !fogged && !wormhole.swallowed()) {
            float rad = ship.rotation * PI / 180.0f;
            float ux = sinf(rad), uy = -cosf(rad);
            float bx = ship.posX * viewScale + viewX;
            float by = ship.posY * viewScale + viewY;
            drawProbe(r, bx, by, ux, uy, ship.scale, viewScale);
        }
        storm.drawBolts(r, viewX, viewY, viewScale);
        if (effectVisible(wormhole.coreX(), wormhole.coreY(), WORMHOLE_OUTER_R))
            wormhole.draw(r, viewX, viewY, viewScale);

        drawDockingPiP(r);

        if (lavaBurn) {
            // The ship touched down on lava: it glows white-hot and melts away
            // from the footpads up over the crash delay.
            float melt = 1.0f - resetTimer / CRASH_RESET_DELAY;
            if (melt < 0.0f) melt = 0.0f;
            if (melt > 1.0f) melt = 1.0f;

            float sx = ship.posX * viewScale + viewX;
            float sy = ship.posY * viewScale + viewY;
            float sc = ship.scale * viewScale;
            float pulse = 0.75f + 0.25f * sinf((float)ship.counter * 0.18f);
            float meltWorld = ship.posY + (14.0f - melt * 19.0f) * ship.scale;
            float meltScreen = meltWorld * viewScale + viewY;

            // Pulsing radial heat glow behind the wreck, growing as it melts.
            float gr = (7.0f + melt * 11.0f) * sc + 2.0f;
            int gb = (int)(140.0f * (0.35f + 0.65f * melt) * pulse);
            for (int yy = (int)(-gr); yy <= (int)gr; yy++) {
                int halfw = (int)sqrtf(gr * gr - (float)(yy * yy));
                for (int xx = -halfw; xx <= halfw; xx++) {
                    float d = sqrtf((float)(xx * xx + yy * yy)) / gr;
                    int b = (int)(gb * (1.0f - d));
                    if (b > 0) r.pixelShade(sx + (float)xx, sy + 2.0f * sc + (float)yy, b);
                }
            }

            // Molten edge across the hull at the melt front.
            int edgeB = (int)(255.0f * pulse);
            int halfw2 = (int)(11.0f * sc);
            for (int xx = -halfw2; xx <= halfw2; xx++) {
                int ax = xx < 0 ? -xx : xx;
                float wob = 1.5f * sinf((float)ship.counter * 0.3f + xx * 0.5f);
                r.pixelShade(sx + (float)xx, meltScreen + wob, edgeB - ax * 3);
            }

            // Embers rising from the glowing wreck.
            for (int e = 0; e < 14; e++) {
                int seed = e * 7 + ship.counter;
                float ex = sx + (float)((seed * 37) % 41 - 20) * 0.35f * sc;
                float ph = (float)((seed * 53) % 100) / 100.0f;
                float life = fmodf((float)ship.counter * 0.02f + ph, 1.0f);
                float rise = life * (9.0f + melt * 12.0f) * sc;
                float sway = sinf((float)ship.counter * 0.3f + ph * 6.0f) * 2.5f * sc;
                int b = (int)(230.0f * (1.0f - life));
                if (b > 0) r.pixelShade(ex + sway, sy - rise, b);
            }

            // Molten drips falling from the melt front.
            for (int d = 0; d < 8; d++) {
                int seed = d * 13 + ship.counter;
                float dx = sx + (float)((seed * 29) % 31 - 15) * 0.5f * sc;
                float ph = (float)((seed * 71) % 100) / 100.0f;
                float life = fmodf((float)ship.counter * 0.015f + ph, 1.0f);
                float fall = life * (16.0f + melt * 10.0f) * sc;
                int b = (int)(200.0f * (1.0f - life));
                if (b > 0) r.pixelShade(dx, meltScreen + fall, b);
            }

            // The hull itself melts away from the bottom up.
            ship.draw(r, viewX, viewY, viewScale, melt);
        } else if (tankerCrash) {
            // Fuel tanker explosion: a fixed-point particle/debris system.
            float sx = ship.posX * viewScale + viewX;
            float sy = ship.posY * viewScale + viewY;
            float sc = ship.scale * viewScale;
            int exx = (int)(sx + 0.5f);
            int eyy = (int)(sy + 2.0f * sc + 0.5f);
            if (!explosionInited) {
                explosion.inicializar(exx, eyy);
                explosionInited = true;
            }
            explosion.actualizarYRenderizar(r);
            ship.draw(r, viewX, viewY, viewScale);
        }

        {
            const std::vector<TerrainLine> &tl = terrain.getLines();
            int blink = (ship.counter / 20) & 1;
            for (int i = 0; i < (int)tl.size(); i++) {
                if (tl[i].labelX < 0 || !tl[i].landable) continue;
                float zx1 = tl[i].x1, zx2 = tl[i].x2;
                int j = i;
                while (j + 1 < (int)tl.size() && tl[j + 1].landable) {
                    j++;
                    zx2 = tl[j].x2;
                }
                float zy = tl[i].y1 + 3.0f;
                float zw = zx2 - zx1;
                int n = (int)(zw / 5.0f);
                if (n < 3) n = 3;
                if (n > 12) n = 12;
                for (int k = 0; k < n; k++) {
                    if (((k + blink) & 1) == 0) continue;
                    float wx = zx1 + zw * (k + 0.5f) / n;
                    r.rect(wx * viewScale + viewX - 1.0f,
                           zy * viewScale + viewY - 1.0f, 2.0f, 2.0f);
                }
                i = j;
            }
        }

        viewX = svx;
        viewY = svy;

        char buf[40];
        if (introTimer <= 0) {
            bool glitch = (stormHitTimer > 0.0f);

            int ang = (int)ship.rotation;
            int pwr = (int)(input.powerLevel * 100);
            int alt = (ship.altitude < 0) ? 0 : (int)ship.altitude;
            int vx = (int)(ship.velX * 200);
            int vy = (int)(ship.velY * 200);

            // A lightning hit scrambles the instruments (EM interference):
            // readouts show random alphanumeric garbage that dances around.
            snprintf(buf, sizeof buf, "L%d SCORE %d", level, score);
            r.text(22, 22, buf);
            snprintf(buf, sizeof buf, "FUEL %d", (int)ship.fuel);
            if (ship.fuel <= 0) {
                if ((ship.counter % 50) < 30) r.text(22, 32, buf);
            } else if (ship.fuel < 300) {
                if ((ship.counter % 50) < 30) r.text(22, 32, buf);
            } else {
                r.text(22, 32, buf);
            }

            // VX/VY labels flash when the horizontal/vertical speed is too
            // high to land safely (the old "TOO FAST" banner is gone).
            bool alarm = (state == STATE_PLAYING && introTimer <= 0);
            bool vxAlarm = alarm && (ship.velX > LAND_HARD_VX || ship.velX < -LAND_HARD_VX);
            bool vyAlarm = alarm && (ship.velY > LAND_HARD_VY);

            if (glitch) {
                char gb[8];
                glitchChars(gb, 3);
                snprintf(buf, sizeof buf, "ANG %s", gb);
                r.text(22, 42, buf);
                glitchChars(gb, 3);
                snprintf(buf, sizeof buf, "PWR %s", gb);
                r.text(22, 52, buf);
                glitchChars(gb, 3);
                snprintf(buf, sizeof buf, "ALT %s", gb);
                r.text(250, 22, buf);
                glitchChars(gb, 3);
                snprintf(buf, sizeof buf, "VX  %s", gb);
                r.text(250, 32, buf);
                glitchChars(gb, 3);
                snprintf(buf, sizeof buf, "VY  %s", gb);
                r.text(250, 42, buf);
                glitchChars(gb, 3);
                snprintf(buf, sizeof buf, "G   %s", gb);
                r.text(250, 52, buf);
                glitchChars(gb, 3);
                snprintf(buf, sizeof buf, "WIND %s", gb);
                r.text(250, 62, buf);
                glitchChars(gb, 3);
                snprintf(buf, sizeof buf, "HULL %s", gb);
                r.text(250, 72, buf);
            } else {
                snprintf(buf, sizeof buf, "ANG %d", ang);
                r.text(22, 42, buf);
                snprintf(buf, sizeof buf, "PWR %d", pwr);
                r.text(22, 52, buf);
                snprintf(buf, sizeof buf, "ALT %d", alt);
                r.text(250, 22, buf);
                bool flashVX = vxAlarm && (ship.counter % 50) >= 30;
                bool flashVY = vyAlarm && (ship.counter % 50) >= 30;
                snprintf(buf, sizeof buf, "VX %d", vx);
                if (!flashVX) r.text(250, 32, buf);
                snprintf(buf, sizeof buf, "VY %d", vy);
                if (!flashVY) r.text(250, 42, buf);
                snprintf(buf, sizeof buf, "G %.2f", ship.gravity / GRAVITY);
                r.text(250, 52, buf);
            }

            if (demo) r.text(22, 62, "DEMO");
#if SHOW_DEBUG_SCALES
            snprintf(buf, sizeof buf, "SCL %.3f", ship.scale);
            r.text(250, 92, buf);
            snprintf(buf, sizeof buf, "VWS %.3f", viewScale);
            r.text(250, 102, buf);
            snprintf(buf, sizeof buf, "TK %.3f", Tanker::drawScaleFor(ship.scale, viewScale));
            r.text(250, 112, buf);
#endif
            bool windShown = windEnabled;
            if (windShown) {
                if (!glitch) {
                    snprintf(buf, sizeof buf, "WIND %d%c", (int)(windStrength * 100.0f),
                             windDir > 0 ? '>' : '<');
                    r.text(250, 62, buf);
                }
                warnY = 72;
            }

            if (acidrain.active()) {
                if (!glitch) {
                    int hv = (int)hullIntegrity;
                    bool hullAlarm = hv <= 10;
                    bool flashHull = hullAlarm && (ship.counter % 40) >= 26;
                    char numBuf[12];
                    snprintf(numBuf, sizeof numBuf, "%d", hv);
                    if (!flashHull) r.text(250, 72, "HULL");
                    r.text(280, 72, numBuf);
                }
                warnY = 82;
            }

            if (quake.phase() == Quake::RUMBLING) {
                bool flashSeismic = (ship.counter % 30) >= 20;
                if (!flashSeismic) r.text(250, 72, "SEISMIC");
            }

            // Parachute status (top-left, above the L<level> line): solid =
            // available, blinking = deployed, "TOO LOW" briefly flashes when a
            // deploy was refused. Nothing is shown once the chute is spent.
            if (chuteAvailable) {
                if (chuteTooLowTimer > 0.0f) {
                    if ((ship.counter % 40) < 26) r.text(22, 72, "TOO LOW");
                } else if (ship.chute) {
                    if ((ship.counter % 30) < 22) r.text(22, 72, "CHUTE");
                } else {
                    r.text(22, 72, "CHUTE");
                }
            }
        }

        auto centerText = [&r](float y, const char *s) {
            r.text((SCREEN_W - (int)strlen(s) * 6) / 2.0f, y, s);
        };

        // Wormhole respawn banner: shown a few seconds after the ship warps to
        // the other moon. Plain centered text, same style as the landing/crash
        // messages.
        if (recycledTimer > 0.0f) {
            centerText(170, "CONGRATULATIONS,");
            centerText(182, "YOU'VE BEEN RECYCLED!");
        }

        if (state == STATE_LANDED) {
            if (landPerfect) {
                centerText(170, "CONGRATULATIONS");
                centerText(182, "PERFECT LANDING");
            } else {
                centerText(176, "GOOD LANDING");
            }
        } else if (state == STATE_CRASHED) {
            if (lavaBurn) {
                centerText(170, "YOU BURNED");
                centerText(182, "LAVA DESTROYED THE SHIP");
            } else if (ringHit) {
                    if (zoomedIn) {
                    float bandSy = rings.centerBandY(terrain, ship.posX) * viewScale + viewY;
                    float yTxt = bandSy + 36.0f;
                    if (yTxt > SCREEN_H - 30.0f) yTxt = SCREEN_H - 30.0f;
                    if (yTxt < 20.0f) yTxt = 20.0f;
                    centerText(yTxt, "YOU CRASHED");
                    centerText(yTxt + 12, "STRUCK BY ORBITAL DEBRIS");
                } else {
                    centerText(170, "YOU CRASHED");
                    centerText(182, "STRUCK BY ORBITAL DEBRIS");
                }
            } else if (twisterCrash) {
                centerText(170, "YOU CRASHED");
                centerText(182, "TWISTER SMASHED THE SHIP");
            } else if (tankerCrash) {
                centerText(170, "BOTH DESTROYED");
                centerText(182, "COLLIDED WITH THE TANKER");
            } else if (quakeCrash) {
                centerText(170, "YOU CRASHED");
                centerText(182, "THE GROUND GAVE WAY");
            } else if (acidBurn) {
                centerText(176, "ACID RAIN CORRODED THE SHIP");
            } else {
                centerText(170, "YOU CRASHED");
                centerText(182, "FUEL TANKS DESTROYED");
            }
        } else if (state == STATE_GAMEOVER) {
            centerText(170, "OUT OF FUEL");
            centerText(182, "GAME OVER");
        }

            if (tanker.docked && tanker.fuelFlowing) {
                r.text(250, warnY, "REFUELING");
            } else if (tanker.docked) {
                if ((ship.counter % 40) < 24) r.text(250, warnY, "DOCKING");
            } else {
                bool tankerZone = tanker.targeted() &&
                    fabsf(ship.posX - tanker.bodyX) < TANKER_DOCK_ZONE_X &&
                    fabsf(ship.posY - tanker.portY) < TANKER_DOCK_ZONE_Y;
                if (tankerZone && (ship.counter % 40) < 24) {
                    r.text(250, warnY, "DOCKING");
                }
            }

            if (fuelMaxTimer > 0.0f) {
            centerText(106, "FUEL MAX");
        }

        if (introTimer > 0) {
            float elapsed = LEVEL_INTRO_TIME - introTimer;
            float b = 1.0f;
            if (elapsed < INTRO_FADE_IN) b = elapsed / INTRO_FADE_IN;
            else if (introTimer < INTRO_FADE_OUT) b = introTimer / INTRO_FADE_OUT;
            int brightness = (int)(b * 255.0f);
            int scale = 3;
            snprintf(buf, sizeof buf, "LEVEL %d", level);
            int tw = (int)strlen(buf) * 6 * scale;
            float tx = (SCREEN_W - tw) / 2.0f;
            float ty = (SCREEN_H - 7 * scale) / 2.0f;
            r.textScaled(tx, ty, buf, (float)scale, brightness);

            int scale2 = 2;
            const char *mn = moonName(level);
            int tw2 = (int)strlen(mn) * 6 * scale2;
            float tx2 = (SCREEN_W - tw2) / 2.0f;
            float ty2 = ty + 7 * scale + 4;
            r.textScaled(tx2, ty2, mn, (float)scale2, brightness);
        }

        bool dockZone = tanker.active && !tanker.done &&
                        fabsf(ship.posX - tanker.bodyX) < TANKER_DOCK_ZONE_X &&
                        fabsf(ship.posY - tanker.portY) < TANKER_DOCK_ZONE_Y;
        bool inApproach = ship.altitude < APPROACH_ALT;
        if (inApproach && !dockZone) {
            const float MX = 112, MY = 22, MW = 96, MH = 49;
            r.line(MX, MY, MX + MW, MY);
            r.line(MX, MY + MH, MX + MW, MY + MH);
            r.line(MX, MY, MX, MY + MH);
            r.line(MX + MW, MY, MX + MW, MY + MH);

            const std::vector<TerrainLine> &tl = terrain.getLines();
            float minTX = tl[0].x1, maxTX = tl[0].x2;
            float minTY = 9999, maxTY = 0;
            for (int i = 0; i < (int)tl.size(); i++) {
                if (tl[i].x1 < minTX) minTX = tl[i].x1;
                if (tl[i].x2 > maxTX) maxTX = tl[i].x2;
                if (tl[i].y1 < minTY) minTY = tl[i].y1;
                if (tl[i].y2 < minTY) minTY = tl[i].y2;
                if (tl[i].y1 > maxTY) maxTY = tl[i].y1;
                if (tl[i].y2 > maxTY) maxTY = tl[i].y2;
            }
            float tw = maxTX - minTX, th = maxTY - minTY;
            float ms = ((MW - 4) / tw < (MH - 4) / th) ? (MW - 4) / tw : (MH - 4) / th;
            float ox = MX + 2 + ((MW - 4) - tw * ms) / 2;
            float oy = MY + 2 + ((MH - 4) - th * ms) / 2;

            for (int i = 0; i < (int)tl.size(); i++) {
                float a = (tl[i].x1 - minTX) * ms + ox;
                float b = (tl[i].y1 - minTY) * ms + oy;
                float c = (tl[i].x2 - minTX) * ms + ox;
                float d = (tl[i].y2 - minTY) * ms + oy;
                r.line(a, b, c, d);
                if (i + 1 < (int)tl.size() && tl[i].x2 == tl[i + 1].x1 && tl[i].y2 != tl[i + 1].y1) {
                    r.line(c, d, (tl[i + 1].x1 - minTX) * ms + ox, (tl[i + 1].y1 - minTY) * ms + oy);
                }
            }

            float smx = (ship.posX - minTX) * ms + ox;
            float smy = (ship.posY - minTY) * ms + oy;
            r.rect(smx - 1, smy - 1, 2, 2);

            for (int i = 0; i < (int)tl.size(); i++) {
                if (tl[i].labelX < 0) continue;
                float ax = (tl[i].labelX - minTX) * ms + ox;
                float ay = (tl[i].y1 - minTY) * ms + oy + 5.0f;
                if (ay + 2 > MY + MH - 1) ay = MY + MH - 3;
                if ((ship.counter / 25) & 1) continue;
                r.rect(ax, ay, 1, 1);
                r.rect(ax - 1, ay + 1, 3, 1);
            }
        }
    }

    r.flush();
}

void Game::drawDockingPiP(Renderer &r)
{
    // Picture-in-picture docking window: a framed sub-screen in the bottom
    // right corner that magnifies the contact point (the drogue basket and
    // the module's probe tip), so millimetric X/Y misalignments are visible
    // that would be lost at the general view's scale.
    if (state != STATE_PLAYING) return;
    if (!tanker.active || tanker.done) return;
    if (introTimer > 0 || lavaBurn) return;
    bool inZone = fabsf(ship.posX - tanker.bodyX) < TANKER_DOCK_ZONE_X &&
                  fabsf(ship.posY - tanker.portY) < TANKER_DOCK_ZONE_Y;
    if (!tanker.docked && !inZone) return;

    float dwx = tanker.drogueX();
    float dwy = tanker.drogueY();
    float rad = ship.rotation * PI / 180.0f;
    float ux = sinf(rad), uy = -cosf(rad);
    // Probe tip = module center + boom along the probe direction (same as
    // drawProbe and checkDock): at rotation 0 (uy=-1) it sits NOZZLE above.
    float pwx = ship.posX + TANKER_NOZZLE_LEN * ship.scale * ux;
    float pwy = ship.posY + TANKER_NOZZLE_LEN * ship.scale * uy;
    float midX = (dwx + pwx) * 0.5f;
    float midY = (dwy + pwy) * 0.5f;

    int P = PIP_SIZE;
    // Top-center, the same spot the minimap uses: while the docking zone is
    // active the main viewport shows the macro zoom-in and the minimap is
    // suppressed, so the window never overlaps it.
    int pipX = (int)((SCREEN_W - P) * 0.5f);
    int pipY = 22;
    // Window center in px: the midpoint between basket and probe tip maps here.
    float cx0 = pipX + P * 0.5f;
    float cy0 = pipY + P * 0.5f;

    // Magnification: full PIP_SCALE when the pair is close (the fine-alignment
    // regime); when they are farther apart than the window can show, zoom out
    // so the basket and probe tip always stay visible (never a black box), with
    // a floor so the window never zooms out past a useful minimum.
    float dist = sqrtf((dwx - pwx) * (dwx - pwx) + (dwy - pwy) * (dwy - pwy));
    float sc = PIP_SCALE;
    float fitHalf = P * 0.5f - 6.0f;
    if (dist > fitHalf / PIP_SCALE) sc = fitHalf / dist;
    if (sc < PIP_SCALE * (1.0f / 3.0f)) sc = PIP_SCALE * (1.0f / 3.0f);

    r.setClip((float)pipX, (float)pipY, (float)P, (float)P);
    // Opaque black background so the magnified view reads clean over the scene.
    for (int yy = pipY; yy < pipY + P; yy++)
        for (int xx = pipX; xx < pipX + P; xx++)
            r.pixelShade((float)xx, (float)yy, 0);

    // Magnified receiving basket (inverted truncated cone; its seat, at the
    // align point, must catch the probe tip).
    float dxx = cx0 + (dwx - midX) * sc;
    float dyy = cy0 + (dwy - midY) * sc;
    Tanker::drawDroguePip(r, dxx, dyy, sc);
    // Magnified probe (thin rod + solid arrowhead tip) reaching toward the basket.
    drawProbe(r, cx0 + (ship.posX - midX) * sc,
              cy0 + (ship.posY - midY) * sc,
              ux, uy, ship.scale, sc);

    // Status ring around the basket gives immediate feedback on the mini-game:
    // red = probe outside and about to break, yellow = aligning/locking,
    // green = locked and fuel is flowing.
    int statusColor = 0;
    if (tanker.docked) {
        if (tanker.fuelFlowing) statusColor = 255;
        else statusColor = 210;
    } else if (inZone) {
        statusColor = 90;
    }
    if (statusColor > 0) {
        float ringR = (PIP_SIZE * 0.5f - 4.0f);
        int segs = 24;
        for (int i = 0; i < segs; i++) {
            float a0 = (float)i / segs * 2.0f * PI;
            float a1 = (float)(i + 1) / segs * 2.0f * PI;
            if ((i + (ship.counter / 4)) % 4 == 0) continue;
            r.lineShade(cx0 + cosf(a0) * ringR, cy0 + sinf(a0) * ringR,
                        cx0 + cosf(a1) * ringR, cy0 + sinf(a1) * ringR,
                        statusColor);
        }
        // A small crosshair shows where the probe tip is relative to the seat.
        float tpx = cx0 + (pwx - midX) * sc;
        float tpy = cy0 + (pwy - midY) * sc;
        r.lineShade(tpx - 2.0f, tpy, tpx + 2.0f, tpy, 160);
        r.lineShade(tpx, tpy - 2.0f, tpx, tpy + 2.0f, 160);
    }

    r.clearClip();

    // White frame delimiting the window.
    r.line(pipX - 3.0f, pipY - 3.0f, pipX + P + 2.0f, pipY - 3.0f);
    r.line(pipX - 3.0f, pipY - 3.0f, pipX - 3.0f, pipY + P + 2.0f);
    r.line(pipX + P + 2.0f, pipY - 3.0f, pipX + P + 2.0f, pipY + P + 2.0f);
    r.line(pipX - 3.0f, pipY + P + 2.0f, pipX + P + 2.0f, pipY + P + 2.0f);
}
