#include <cmath>
#include <cstdlib>
#include <cstring>
#include "ship.h"
#include "config.h"
#include "renderer.h"

Ship::Ship()
    : posX(0), posY(0), velX(0), velY(0),
      rotation(-90), targetRotation(-90),
      thrustBuild(0), fuel(FUEL_MAX), scale(1.0f),
      altitude(0),     active(true), exploding(false), fuelExplosion(false), counter(0),
      windStrength(0), windDir(1), gravity(GRAVITY)
{
    defineShapes();
    memset(shapePosX, 0, sizeof(shapePosX));
    memset(shapePosY, 0, sizeof(shapePosY));
}

void Ship::defineShapes()
{
    // Shape 0 — Ascent stage: tapered body
    shapes[0].dx[0] = -2.0f; shapes[0].dy[0] = -7.0f;
    shapes[0].dx[1] =  2.0f; shapes[0].dy[1] = -7.0f;
    shapes[0].dx[2] =  4.0f; shapes[0].dy[2] =  0.0f;
    shapes[0].dx[3] = -4.0f; shapes[0].dy[3] =  0.0f;
    shapes[0].count = 4;
    shapes[0].closed = true;
    shapes[0].velX = 1.0f; shapes[0].velY = -3.0f;

    // Shape 1 — Descent stage: trapezoid base
    shapes[1].dx[0] = -4.0f; shapes[1].dy[0] =  0.0f;
    shapes[1].dx[1] =  4.0f; shapes[1].dy[1] =  0.0f;
    shapes[1].dx[2] =  5.5f; shapes[1].dy[2] =  5.0f;
    shapes[1].dx[3] = -5.5f; shapes[1].dy[3] =  5.0f;
    shapes[1].count = 4;
    shapes[1].closed = true;
    shapes[1].velX = 0.5f; shapes[1].velY = 1.0f;

    // Shape 2 — Left landing leg + footpad
    shapes[2].dx[0] = -4.5f; shapes[2].dy[0] =  5.0f;
    shapes[2].dx[1] = -6.5f; shapes[2].dy[1] =  8.5f;
    shapes[2].dx[2] = -8.0f; shapes[2].dy[2] =  8.5f;
    shapes[2].dx[3] = -5.5f; shapes[2].dy[3] =  8.5f;
    shapes[2].count = 4;
    shapes[2].closed = false;
    shapes[2].velX = -1.0f; shapes[2].velY = -3.0f;

    // Shape 3 — Right landing leg + footpad
    shapes[3].dx[0] =  4.5f; shapes[3].dy[0] =  5.0f;
    shapes[3].dx[1] =  6.5f; shapes[3].dy[1] =  8.5f;
    shapes[3].dx[2] =  8.0f; shapes[3].dy[2] =  8.5f;
    shapes[3].dx[3] =  5.5f; shapes[3].dy[3] =  8.5f;
    shapes[3].count = 4;
    shapes[3].closed = false;
    shapes[3].velX = 1.0f; shapes[3].velY = -3.0f;

    // Shape 4 — Left thruster nozzle
    shapes[4].dx[0] = -2.5f; shapes[4].dy[0] =  5.0f;
    shapes[4].dx[1] = -4.0f; shapes[4].dy[1] =  9.0f;
    shapes[4].dx[2] = -3.0f; shapes[4].dy[2] = 10.0f;
    shapes[4].count = 3;
    shapes[4].closed = false;
    shapes[4].velX = 1.0f; shapes[4].velY = -1.5f;

    // Shape 5 — Right thruster nozzle
    shapes[5].dx[0] =  2.5f; shapes[5].dy[0] =  5.0f;
    shapes[5].dx[1] =  4.0f; shapes[5].dy[1] =  9.0f;
    shapes[5].dx[2] =  3.0f; shapes[5].dy[2] = 10.0f;
    shapes[5].count = 3;
    shapes[5].closed = false;
    shapes[5].velX = 2.5f; shapes[5].velY = -1.5f;
}

void Ship::reset(float x, float y)
{
    posX = x;
    posY = y;
    velX = 0.415f;
    velY = 0;
    rotation = 0;
    targetRotation = 0;
    thrustBuild = 0;
    fuel = FUEL_MAX;
    scale = 1.0f;
    active = true;
    exploding = false;
    fuelExplosion = false;
    counter = 0;
    windStrength = 0;
    windDir = 1;
    gravity = GRAVITY;
    chute = false;
    chuteOpen = 0;
    for (int i = 0; i < 6; i++) {
        shapePosX[i] = 0;
        shapePosY[i] = 0;
    }
    for (int i = 0; i < GROUND_PARTICLES_MAX; i++) groundParticles[i].active = false;
}

static void shadedLine(Renderer &r, float cx, float y, float halfW, int brightness);

void Ship::setTargetRotation(float deg)
{
    if (deg < ROTATION_MIN_DEG) deg = ROTATION_MIN_DEG;
    if (deg > ROTATION_MAX_DEG) deg = ROTATION_MAX_DEG;
    targetRotation = deg;
}

void Ship::setThrust(float power)
{
    if (power < 0) power = 0;
    if (power > 1) power = 1;
    thrustBuild += (power - thrustBuild) * 0.4f;
    if (thrustBuild < 0.01f) thrustBuild = 0;
}

void Ship::update()
{
    counter++;

    rotation += (targetRotation - rotation) * ROTATION_LERP;
    if (fabsf(rotation - targetRotation) < 0.1f) rotation = targetRotation;

    if (exploding) {
        updateExplosion();
        return;
    }

    if (!active) return;

    if (fuel <= 0) {
        thrustBuild = 0;
    }

    if (thrustBuild > 0) {
        float rad = rotation * PI / 180.0f;
        velX += THRUST_ACCEL * thrustBuild * sinf(rad);
        velY -= THRUST_ACCEL * thrustBuild * cosf(rad);
        fuel -= FUEL_PER_THRUST * thrustBuild;
    }

    if (windStrength > 0.0f) {
        float gain = chute ? PARACHUTE_WIND_GAIN : 1.0f;
        velX += windDir * windStrength * WIND_ACCEL * gain;
    }

    posX += velX;
    posY += velY;
    velX *= DRAG;
    velY += gravity;

    if (velY > TOP_SPEED) velY = TOP_SPEED;
    else if (velY < -TOP_SPEED) velY = -TOP_SPEED;
    if (velX > TOP_SPEED) velX = TOP_SPEED;
    else if (velX < -TOP_SPEED) velX = -TOP_SPEED;

    // Parachute: canopy inflates over PARACHUTE_OPEN_TIME, braking the fall
    // toward the sink only while descending too fast (so a short engine flare
    // can still push below the sink for a perfect landing). The canopy also
    // caps the horizontal drift (a sail: wind gain handled above). Gravity is
    // cancelled inside the brake so the terminal velocity really is the sink.
    if (chute) {
        if (chuteOpen < 1.0f) {
            chuteOpen += GAME_DT / PARACHUTE_OPEN_TIME;
            if (chuteOpen > 1.0f) chuteOpen = 1.0f;
        }
        if (velY > PARACHUTE_SINK) {
            velY += (PARACHUTE_SINK - velY) * 0.15f * chuteOpen - gravity;
        }
        if (velX > PARACHUTE_DRIFT_MAX) velX = PARACHUTE_DRIFT_MAX;
        else if (velX < -PARACHUTE_DRIFT_MAX) velX = -PARACHUTE_DRIFT_MAX;
    }

    left = posX - 10.0f * scale;
    right = posX + 10.0f * scale;
    bottom = posY + 14.0f * scale;
    top = posY - 5.0f * scale;

    if (fuel < 0) fuel = 0;
}

void Ship::draw(Renderer &r, float viewX, float viewY, float viewScale, float melt)
{
    float sx = posX * viewScale + viewX;
    float sy = posY * viewScale + viewY;
    float rad = rotation * PI / 180.0f;
    float cs = cosf(rad);
    float sn = sinf(rad);
    float sc = scale * viewScale;

    // Melt front rises from the footpads (dy=+14) to the top (dy=-5).
    float meltScreen = 0.0f;
    if (melt > 0.0f) {
        float meltWorld = posY + (10.0f - melt * 17.0f) * scale;
        meltScreen = meltWorld * viewScale + viewY;
    }

    for (int s = 0; s < 6; s++) {
        const ShipShape &sh = shapes[s];
        float ox = shapePosX[s] * viewScale;
        float oy = shapePosY[s] * viewScale;

        float vx[8], vy[8];
        for (int i = 0; i < sh.count; i++) {
            vx[i] = sx + (sh.dx[i] * cs - sh.dy[i] * sn) * sc + ox;
            vy[i] = sy + (sh.dx[i] * sn + sh.dy[i] * cs) * sc + oy;
        }

        if (sh.closed && melt <= 0.0f) {
            int bright = (s == 0) ? 140 : 100;
            r.fillPolygon(vx, vy, sh.count, bright);
        }

        for (int i = 0; i < sh.count; i++) {
            int ni = (i + 1) % sh.count;
            if (!sh.closed && i == sh.count - 1) continue;

            if (melt > 0.0f) {
                float wob = 2.0f * sinf((float)((int)(vx[i] + vx[ni])) * 0.37f + (float)counter * 0.35f);
                float mline = meltScreen + wob;
                if (vy[i] <= mline && vy[ni] <= mline) {
                    r.line(vx[i], vy[i], vx[ni], vy[ni]);
                } else if (vy[i] <= mline || vy[ni] <= mline) {
                    float t = (mline - vy[i]) / (vy[ni] - vy[i]);
                    float ix = vx[i] + t * (vx[ni] - vx[i]);
                    if (vy[i] <= mline) r.line(vx[i], vy[i], ix, mline);
                    else r.line(ix, mline, vx[ni], vy[ni]);
                }
                continue;
            }

            r.line(vx[i], vy[i], vx[ni], vy[ni]);
        }
    }

    if (exploding) {
        for (int i = 0; i < GROUND_PARTICLES_MAX; i++) {
            const GroundParticle &p = groundParticles[i];
            if (!p.active) continue;
            float px = p.x * viewScale + viewX;
            float py = p.y * viewScale + viewY;
            float t = p.life / GROUND_PARTICLE_LIFE;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            int b = (int)(200.0f * t * p.shade);
            int c = (int)(255.0f * t * p.shade);
            if (p.size >= 3.0f) {
                r.pixelShade(px - 1.0f, py - 1.0f, b);
                r.pixelShade(px, py - 1.0f, b);
                r.pixelShade(px + 1.0f, py - 1.0f, b);
                r.pixelShade(px - 1.0f, py, b);
                r.pixelShade(px + 1.0f, py, b);
                r.pixelShade(px - 1.0f, py + 1.0f, b);
                r.pixelShade(px, py + 1.0f, b);
                r.pixelShade(px + 1.0f, py + 1.0f, b);
                r.pixelShade(px, py, c);
            } else if (p.size >= 2.0f) {
                r.pixelShade(px - 1.0f, py, b);
                r.pixelShade(px + 1.0f, py, b);
                r.pixelShade(px, py - 1.0f, b);
                r.pixelShade(px, py + 1.0f, b);
                r.pixelShade(px, py, c);
            } else {
                r.pixelShade(px, py, c);
            }
        }
    }

    if (thrustBuild > 0 && active) {
        float flameLen = thrustBuild * 24.0f;
        float fx1 = sx + (-1.5f * cs - 5.0f * sn) * sc;
        float fy1 = sy + (-1.5f * sn + 5.0f * cs) * sc;
        float fx3 = sx + (1.5f * cs - 5.0f * sn) * sc;
        float fy3 = sy + (1.5f * sn + 5.0f * cs) * sc;
        float tx = sx + (0 * cs - (5.0f + flameLen) * sn) * sc;
        float ty = sy + (0 * sn + (5.0f + flameLen) * cs) * sc;

        float bend = (float)windDir * windStrength * flameLen * sc * 0.7f;
        tx += bend;
        fx1 += bend * 0.3f;
        fx3 += bend * 0.3f;

        float cx = 0.5f * (fx1 + fx3), cy = 0.5f * (fy1 + fy3);
        float ax = tx - cx, ay = ty - cy;
        float alen = sqrtf(ax * ax + ay * ay);
        if (alen < 0.5f) alen = 0.5f;
        float ux = ax / alen, uy = ay / alen;
        float px2 = -uy, py2 = ux;
        float ddx = fx3 - fx1, ddy = fy3 - fy1;
        float width0 = sqrtf(ddx * ddx + ddy * ddy);

        int rows = 10;
        for (int i = 1; i <= rows; i++) {
            float tt = (float)i / (rows + 1);
            float d = tt * alen;
            // Tear-drop profile: narrow at the nozzle (base) and tip, widest
            // mid-flame. Keeps the start hugging the body yet never overlaps
            // it when the ship rotates.
            float halfW = 0.5f * width0 * (tt * (1.0f - tt)) * 4.0f;
            int b = (int)(255.0f * (1.0f - 0.6f * tt));
            float x0 = cx + ux * d - px2 * halfW;
            float y0 = cy + uy * d - py2 * halfW;
            float x1 = cx + ux * d + px2 * halfW;
            float y1 = cy + uy * d + py2 * halfW;
            int steps = (int)roundf(fabsf(x1 - x0) + fabsf(y1 - y0));
            if (steps < 1) steps = 1;
            for (int s = 0; s <= steps; s++) {
                float u = (float)s / steps;
                r.pixelShade(x0 + (x1 - x0) * u, y0 + (y1 - y0) * u, b);
            }
        }

        r.line(cx, cy, tx, ty);

        if (windStrength > 0.05f) {
            int tb = (int)(90.0f * windStrength);
            float ex = tx + (float)windDir * flameLen * sc * 0.6f * windStrength;
            float ey = ty + flameLen * sc * 0.15f * windStrength;
            for (int k = 1; k <= 4; k++) {
                float x = tx + (ex - tx) * k / 4.0f;
                float y = ty + (ey - ty) * k / 4.0f;
                int b = tb - k * 12;
                if (b < 0) b = 0;
                r.pixelShade(x, y, b);
            }
        }
    }

    // Parachute: a folded pack sits on the hull while available; when deployed
    // a modern steerable (ram-air) canopy spreads well above the ship — wide,
    // flat-ish wing with two shroud lines per side converging at each hull
    // corner (the classic steerable V). Points are ship-local so the canopy
    // rides the ship's orientation (auto-leveled to 0 while open). The canopy
    // scales around the hull top (dy=-5) as chuteOpen ramps 0..1.
    const float CANOPY_W = 18.0f;    // rim half width (world u)
    const float CANOPY_H = 9.0f;     // dome rise above the rim (world u)
    const float CANOPY_RIM = -16.0f; // canopy bottom edge, well clear of the hull
    if (!chute) {
        float px = sx + (0.0f * cs - (-5.5f) * sn) * sc;
        float py = sy + (0.0f * sn + (-5.5f) * cs) * sc;
        r.rect(px - 2.0f * sc, py - 1.0f * sc, 4.0f * sc, 2.0f * sc);
        r.line(px - 2.0f * sc, py - 1.0f * sc, px - 2.0f * sc, py + 1.0f * sc);
        r.line(px + 2.0f * sc, py - 1.0f * sc, px + 2.0f * sc, py + 1.0f * sc);
    } else {
        float open = chuteOpen;
        if (open <= 0.0f) open = 0.02f;
        auto pt = [&](float dx, float dy, float &ox, float &oy) {
            float wx = dx * open;
            float wy = -5.0f + (dy + 5.0f) * open;
            ox = sx + (wx * cs - wy * sn) * sc;
            oy = sy + (wx * sn + wy * cs) * sc;
        };

        // Shroud lines: two lines per side running from each rim end down to
        // the hull corner (steerable canopy risers).
        {
            float axL, ayL, axR, ayR;
            pt(-5.0f, -2.6f, axL, ayL);
            pt(5.0f, -2.6f, axR, ayR);
            for (int s = -1; s <= 1; s += 2) {
                float x1, y1, x2, y2;
                pt(CANOPY_W * s, CANOPY_RIM, x1, y1);
                pt(CANOPY_W * 0.55f * s, CANOPY_RIM, x2, y2);
                float ax = (s < 0) ? axL : axR;
                float ay = (s < 0) ? ayL : ayR;
                r.lineShade(x1, y1, ax, ay, 120);
                r.lineShade(x2, y2, ax, ay, 120);
            }
        }

        // Soft canopy body fill (faint, so the arcade grey reads as cloth).
        float rimX, rimY, topX, topY;
        pt(0, CANOPY_RIM, rimX, rimY);
        pt(0, CANOPY_RIM - CANOPY_H, topX, topY);
        float wRim = CANOPY_W * open * sc;
        int rows = (int)(fabsf(topY - rimY));
        if (rows < 1) rows = 1;
        for (int k = 1; k < rows; k++) {
            float t = (float)k / rows;
            float hw = wRim * sqrtf(1.0f - t * t);
            int b = (int)(30.0f * (1.0f - t * 0.5f) * open);
            shadedLine(r, rimX, rimY + (topY - rimY) * t, hw, b);
        }

        // Ram-air cells: faint vertical dividers under the dome.
        int cells = 6;
        for (int c = 1; c < cells; c++) {
            float fx = -CANOPY_W + 2.0f * CANOPY_W * c / cells;
            float x1, y1, x2, y2;
            pt(fx, CANOPY_RIM, x1, y1);
            pt(fx * 0.92f, CANOPY_RIM - CANOPY_H * 0.7f, x2, y2);
            r.lineShade(x1, y1, x2, y2, 40);
        }

        // Dome edge: a wide, flat elliptical arc (paraglider profile) over the
        // rim. Two mirrored halves trace dx 0..+W and 0..-W.
        int seg = 10;
        for (int k = 0; k < seg; k++) {
            float a0 = (float)k / seg, a1 = (float)(k + 1) / seg;
            float xa, ya, xb, yb;
            pt(CANOPY_W * sinf(a0 * PI * 0.5f),
               CANOPY_RIM - CANOPY_H * cosf(a0 * PI * 0.5f), xa, ya);
            pt(CANOPY_W * sinf(a1 * PI * 0.5f),
               CANOPY_RIM - CANOPY_H * cosf(a1 * PI * 0.5f), xb, yb);
            r.line(xa, ya, xb, yb);
            pt(-CANOPY_W * sinf(a0 * PI * 0.5f),
               CANOPY_RIM - CANOPY_H * cosf(a0 * PI * 0.5f), xa, ya);
            pt(-CANOPY_W * sinf(a1 * PI * 0.5f),
               CANOPY_RIM - CANOPY_H * cosf(a1 * PI * 0.5f), xb, yb);
            r.line(xa, ya, xb, yb);
        }

        // Scalloped rim: a row of small arcs so the canopy reads as cloth.
        int bumps = 4;
        for (int b = 0; b < bumps; b++) {
            float xc = -CANOPY_W + (2.0f * CANOPY_W) * (b + 0.5f) / bumps;
            float hw = CANOPY_W / bumps;
            float x1, y1, x2, y2, x3, y3;
            pt(xc - hw * 0.5f, CANOPY_RIM, x1, y1);
            pt(xc, CANOPY_RIM - 1.5f, x2, y2);
            pt(xc + hw * 0.5f, CANOPY_RIM, x3, y3);
            r.line(x1, y1, x2, y2);
            r.line(x2, y2, x3, y3);
        }
    }
}

static void shadedLine(Renderer &r, float cx, float y, float halfW, int brightness)
{
    int a = (int)roundf(cx - halfW), b = (int)roundf(cx + halfW);
    for (int x = a; x <= b; x++) r.pixelShade((float)x, y, brightness);
}

void Ship::initGroundParticles()
{
    float gy = posY + 14.0f * scale;
    float mul = fuelExplosion ? 2.5f : 1.0f;
    for (int i = 0; i < GROUND_PARTICLES_MAX; i++) {
        GroundParticle &p = groundParticles[i];
        p.x = posX + ((rand() % 200) - 100) / 100.0f;
        p.y = gy;
        p.velX = (velX * 0.4f + ((rand() % 700) - 350) / 1000.0f) * mul;
        p.velY = -(((rand() % 250) + 80) / 1000.0f + velY * 0.5f) * mul;
        int sz = rand() % 10;
        p.size = (sz < 4) ? 1.0f : (sz < 8) ? 2.0f : 3.0f;
        p.shade = 0.7f + (float)(rand() % 30) / 100.0f;
        p.life = GROUND_PARTICLE_LIFE - (float)(rand() % 12);
        p.active = true;
    }
}

void Ship::crash(bool fuel)
{
    rotation = 0;
    targetRotation = 0;
    active = false;
    exploding = true;
    fuelExplosion = fuel;
    thrustBuild = 0;
    chute = false;
    chuteOpen = 0;
    initGroundParticles();
}

void Ship::land()
{
    active = false;
    thrustBuild = 0;
}

void Ship::updateExplosion()
{
    float mul = fuelExplosion ? 2.5f : 1.0f;
    for (int i = 0; i < 6; i++) {
        shapePosX[i] += shapes[i].velX * 0.1f * mul;
        shapePosY[i] += shapes[i].velY * 0.1f * mul;
    }
    for (int i = 0; i < GROUND_PARTICLES_MAX; i++) {
        GroundParticle &p = groundParticles[i];
        if (!p.active) continue;
        p.velY += GROUND_PARTICLE_GRAV * (gravity / GRAVITY);
        p.x += p.velX;
        p.y += p.velY;
        p.life -= 1.0f;
        if (p.life <= 0.0f) p.active = false;
    }
}
