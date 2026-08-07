#include <cmath>
#include <cstdio>
#include <cstring>
#include "game.h"
#include "moons.h"

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
      viewX(0), viewY(0), viewScale(1.0f),
      zoomedIn(false), resetTimer(0), landMultiplier(1),
      demoSkill(1.0f), demoTargetX(0), demoTargetY(0),
      windPhase(0), windFlipTimer(0), stormHitTimer(0), lavaBurn(false)
{
    input.startPressed = false;
    input.angle = 0;
    input.thrust = 0;
    input.powerLevel = 0;
    terrain.init();
    storm.reset(level);
    geysers.reset(level, terrain);
    volcanoes.reset(level, terrain);
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
    state = STATE_PLAYING;
    ship.reset(110, 150);
    setZoom(false);
    resetTimer = 0;
    introTimer = LEVEL_INTRO_TIME;
    ship.velX = 0.415f;
    if (level <= 1) terrain.init();
    else terrain.generate(level);
    windEnabled = (level >= WIND_START_LEVEL) &&
                  (rand() % 100) < WIND_CHANCE_PERCENT;
    spawnWind();
    storm.reset(level);
    geysers.reset(level, terrain);
    volcanoes.reset(level, terrain);
    stormHitTimer = 0;
    lavaBurn = false;
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
                  (rand() % 100) < WIND_CHANCE_PERCENT;
    spawnWind();
    storm.reset(level);
    geysers.reset(level, terrain);
    volcanoes.reset(level, terrain);
    stormHitTimer = 0;
    lavaBurn = false;
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
    if (rand() % 100 < 50) demoSkill = (float)(rand() % 36) / 100.0f;
    else demoSkill = 0.6f + (float)(rand() % 41) / 100.0f;
    level = (DEMO_LEVEL_FORCE > 0) ? DEMO_LEVEL_FORCE : 1 + rand() % DEMO_MAX_LEVEL;
    score = 0;
    fuel = FUEL_MAX;
    ship.fuel = FUEL_MAX;
    state = STATE_PLAYING;
    if (level <= 1) terrain.init();
    else terrain.generate(level);
    windEnabled = (level >= WIND_START_LEVEL) &&
                  (rand() % 100) < WIND_CHANCE_PERCENT;
    spawnWind();
    storm.reset(level);
    geysers.reset(level, terrain);
    volcanoes.reset(level, terrain);
    stormHitTimer = 0;
    lavaBurn = false;
    ship.reset(110, 150);
    ship.velX = 0.06f;
    setZoom(false);
    resetTimer = 0;
    introTimer = LEVEL_INTRO_TIME;

    const std::vector<TerrainLine> &tl = terrain.getLines();

    // TEST aim: prefer a lava-covered strip of a landing pad (Io), so the
    // burnt-ship ending shows up while tuning it. Pick the lava zone closest
    // to the spawn so the flight is short and cannot land short on an
    // intervening pad. Regenerate the forced level until lava is available;
    // otherwise fall back to any landing pad.
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
        geysers.reset(level, terrain);
        volcanoes.reset(level, terrain);
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

void Game::endDemoToTitle()
{
    state = STATE_WAITING;
    demoTimer = DEMO_START_DELAY;
    terrain.init();
    setZoom(false);
    setupTitleShip();
}

void Game::setupTitleShip()
{
    ship.reset(110, 150);
    ship.velX = -0.35f;
    ship.posX = (SCREEN_W - 20.0f) / viewScale;
}

void Game::runDemoAI()
{
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

    float imp = 1.0f - demoSkill;
    float n = (float)(rand() % 1001) / 1000.0f - 0.5f;
    angle += n * imp * 40.0f;
    thrust = clampf(thrust + n * imp * 0.25f, 0.0f, 1.0f);

    input.angle = clampf(angle, -90.0f, 90.0f) * (PI / 180.0f);
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

void Game::setZoom(bool zoom)
{
    if (zoom) {
        viewScale = SCREEN_H / 700.0f * 5.0f;
        zoomedIn = true;
        viewX = -ship.posX * viewScale + SCREEN_W / 2.0f;
        viewY = -ship.posY * viewScale + SCREEN_H * 0.25f;
        ship.scale = 0.32f;
    } else {
        viewScale = SCREEN_H / 700.0f;
        zoomedIn = false;
        ship.scale = 1.0f;
        viewX = 0;
        viewY = 0;
    }
}

void Game::updateView()
{
    float margintop = SCREEN_H * 0.2f;
    float marginbottom = SCREEN_H * 0.3f;
    float marginx = SCREEN_W * 0.2f;

    if (!zoomedIn && ship.altitude < ZOOM_IN_ALT) {
        setZoom(true);
    } else if (zoomedIn && ship.altitude > ZOOM_OUT_ALT) {
        setZoom(false);
    }

    float sx = ship.posX * viewScale + viewX;
    if (sx < marginx) viewX = -ship.posX * viewScale + marginx;
    else if (sx > SCREEN_W - marginx) viewX = -ship.posX * viewScale + SCREEN_W - marginx;

    float sy = ship.posY * viewScale + viewY;
    if (sy < margintop) viewY = -ship.posY * viewScale + margintop;
    else if (sy > SCREEN_H - marginbottom) viewY = -ship.posY * viewScale + SCREEN_H - marginbottom;
}

void Game::checkCollisions()
{
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
        if (ship.velY < LAND_PERFECT_VY) {
            score += (int)(50 * mult);
            fuel += 50;
            ship.fuel += 50;
            if (fuel > FUEL_MAX) fuel = FUEL_MAX;
            if (ship.fuel > FUEL_MAX) ship.fuel = FUEL_MAX;
        } else {
            score += (int)(15 * mult);
        }
        state = STATE_LANDED;
        resetTimer = CRASH_RESET_DELAY;
    } else if (result == 1) {
        int lost = 200 + (rand() % 200);
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
    ship.windStrength = windEnabled ? windStrength : 0.0f;
    ship.windDir = windDir;
    ship.gravity = GRAVITY * moonGravity(level);
    if (state != STATE_WAITING) storm.update(dt, terrain);
    if (state != STATE_WAITING) geysers.update(dt);
    if (state != STATE_WAITING) volcanoes.update(dt);

    if (input.startPressed && demo) {
        demo = false;
        newGame();
        return;
    }

    if (state == STATE_WAITING) {
        ship.update();
        ship.altitude = terrain.getLines()[0].y1 - ship.bottom;
        if (input.startPressed) {
            newGame();
        } else {
            demoTimer -= dt;
            if (demoTimer <= 0) startDemo();
        }
        return;
    }

    if (state == STATE_PLAYING) {
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

        if (demo) runDemoAI();

        float deg = input.angle * 180.0f / PI;
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
        ship.update();
        if (geysers.inPlume(ship.posX, ship.posY)) ship.velY -= GEYSER_PUSH;

        if (ship.posX > terrain.getWidth() + 10)
            ship.posX = -10;
        else if (ship.posX < -10)
            ship.posX = terrain.getWidth() + 10;

        if (ship.posY < -50) {
            ship.reset(110, 150);
            ship.velX = 2;
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
        checkCollisions();
        return;
    }

    if (state == STATE_LANDED || state == STATE_CRASHED) {
        ship.update();
        resetTimer -= dt;
        if (resetTimer <= 0) {
            if (demo) {
                endDemoToTitle();
            } else if (ship.fuel <= 0) {
                endGame();
            } else if (state == STATE_LANDED) {
                nextLevel();
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
            setupTitleShip();
        }
    }
}

void Game::draw(Renderer &r)
{
    r.clear();

    if (state != STATE_WAITING) storm.drawSky(r, viewX, viewY, viewScale);

    int warnY = 62, fastY = 72;

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

        if ((ship.counter % 50) < 30) r.text(103, 183, "PRESS BUTTON TO PLAY");

        auto SX = [](float x) { return x * 1.2f + 26.0f; };
        auto SY = [](float y) { return y * 1.2f + 56.0f; };

        // Descent stage: octagonal base and landing legs.
        r.line(SX(40), SY(55), SX(60), SY(55));
        r.line(SX(40), SY(55), SX(30), SY(80));
        r.line(SX(60), SY(55), SX(70), SY(80));
        r.line(SX(30), SY(80), SX(70), SY(80));

        r.line(SX(50), SY(80), SX(50), SY(95));
        r.circle(SX(50), SY(95), 2.4f);
        r.line(SX(30), SY(80), SX(15), SY(90));
        r.line(SX(15), SY(90), SX(5), SY(95));
        r.circle(SX(5), SY(95), 1.8f);
        r.line(SX(70), SY(80), SX(85), SY(90));
        r.line(SX(85), SY(90), SX(95), SY(95));
        r.circle(SX(95), SY(95), 1.8f);

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

        // Ascent stage: body, central panel and side boxes.
        r.line(SX(35), SY(55), SX(35), SY(30));
        r.line(SX(65), SY(55), SX(65), SY(30));
        r.line(SX(35), SY(30), SX(45), SY(15));
        r.line(SX(65), SY(30), SX(55), SY(15));
        r.line(SX(45), SY(15), SX(55), SY(15));

        r.line(SX(43), SY(30), SX(57), SY(30));
        r.line(SX(43), SY(30), SX(40), SY(50));
        r.line(SX(57), SY(30), SX(60), SY(50));
        r.line(SX(40), SY(50), SX(60), SY(50));
        r.rect(SX(48), SY(35), 4.8f, 12.0f);

        r.rect(SX(28), SY(38), 8.4f, 9.6f);
        r.line(SX(28), SY(38), SX(26), SY(40));
        r.rect(SX(65), SY(38), 8.4f, 9.6f);
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
    } else {
        terrain.draw(r, viewX, viewY, viewScale, ship.counter);
        geysers.draw(r, viewX, viewY, viewScale);
        volcanoes.draw(r, viewX, viewY, viewScale);
        drawWind(r);
        if (!lavaBurn) ship.draw(r, viewX, viewY, viewScale);
        storm.drawBolts(r, viewX, viewY, viewScale);

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
        }

        {
            const std::vector<TerrainLine> &tl = terrain.getLines();
            int blink = (ship.counter / 20) & 1;
            for (int i = 0; i < (int)tl.size(); i++) {
                if (tl[i].labelX < 0) continue;
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
            r.text(22, 32, buf);

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
            } else {
                snprintf(buf, sizeof buf, "ANG %d", ang);
                r.text(22, 42, buf);
                snprintf(buf, sizeof buf, "PWR %d", pwr);
                r.text(22, 52, buf);
                snprintf(buf, sizeof buf, "ALT %d", alt);
                r.text(250, 22, buf);
                snprintf(buf, sizeof buf, "VX %d", vx);
                r.text(250, 32, buf);
                snprintf(buf, sizeof buf, "VY %d", vy);
                r.text(250, 42, buf);
                snprintf(buf, sizeof buf, "G %.2f", ship.gravity / GRAVITY);
                r.text(250, 52, buf);
            }

            if (demo) r.text(22, 62, "DEMO");
            bool windShown = windEnabled;
            if (windShown) {
                snprintf(buf, sizeof buf, "WIND %d%c", (int)(windStrength * 100.0f),
                         windDir > 0 ? '>' : '<');
                r.text(250, 62, buf);
                warnY = 72;
                fastY = 82;
            }
        }

        auto centerText = [&r](float y, const char *s) {
            r.text((SCREEN_W - (int)strlen(s) * 6) / 2.0f, y, s);
        };

        if (state == STATE_LANDED) {
            if (ship.velY < LAND_PERFECT_VY) {
                centerText(90, "CONGRATULATIONS");
                centerText(102, "PERFECT LANDING");
            } else {
                centerText(90, "HARD LANDING");
                centerText(102, "HOPELESSLY MAROONED");
            }
        } else if (state == STATE_CRASHED) {
            if (lavaBurn) {
                centerText(90, "YOU BURNED");
                centerText(102, "LAVA DESTROYED THE SHIP");
            } else {
                centerText(90, "YOU CRASHED");
                centerText(102, "FUEL TANKS DESTROYED");
            }
        } else if (state == STATE_GAMEOVER) {
            centerText(90, "OUT OF FUEL");
            centerText(102, "GAME OVER");
        }

        if (state == STATE_PLAYING && introTimer <= 0) {
            if (ship.fuel <= 0) {
                if ((ship.counter % 50) < 30) r.text(250, warnY, "OUT OF FUEL");
            } else if (ship.fuel < 300) {
                if ((ship.counter % 50) < 30) r.text(250, warnY, "LOW FUEL");
            }
            if ((ship.velY > LAND_HARD_VY ||
                 ship.velX > LAND_HARD_VX || ship.velX < -LAND_HARD_VX) &&
                (ship.counter % 50) < 30) {
                r.text(250, fastY, "TOO FAST");
            }
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

        if (zoomedIn) {
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
            r.rect(smx - 1, smy - 1, 3, 3);

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
