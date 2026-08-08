#include <cstdlib>
#include <cmath>
#include "tanker.h"
#include "config.h"
#include "moons.h"

Tanker::Tanker()
    : active(false), docked(false), leaving(false), done(false),
      bodyX(0), bodyY(0), baseY(0), originalBodyX(0), portY(0),
      driftX(0), phase(0), droguePhase(0)
{
}

void Tanker::reset(int level, const Terrain& terrain, float fuel, bool force)
{
    active = false;
    docked = false;
    leaving = false;
    done = false;
    droguePhase = 0;
    dockLockTimer = 0.0f;
    dockBreakTimer = 0.0f;
    fuelFlowing = false;

    if (level < TANKER_START_LEVEL && !(TANKER_FORCE_LEVEL1 && level == 1)) return;
    // The air tanker is only useful on a low tank: skip it when fuel is still
    // above half (unless forced, e.g. the attract-mode showcase).
    if (!force && fuel >= FUEL_MAX * TANKER_FUEL_FRACTION) return;
    // Ganymede's debris rings make a high-altitude rendezvous impossible.
    if (moonHasRings(level)) return;
    if (!force && rand() % 100 >= TANKER_CHANCE_PERCENT) return;

    const std::vector<TerrainLine>& tl = terrain.getLines();
    float w = terrain.getWidth();
    float halfW = TANKER_PLATFORM_W * 0.5f;

    bool found = false;
    for (int trial = 0; trial < 60 && !found; trial++) {
        float cx = 280.0f + (float)(rand() % (int)(w - 360.0f));
        float terrainY = terrain.yAt(cx, 500.0f);
        float yL = terrain.yAt(cx - halfW - 2.0f, terrainY);
        float yR = terrain.yAt(cx + halfW + 2.0f, terrainY);
        if (fabsf(yL - terrainY) > 10.0f) continue;
        if (fabsf(yR - terrainY) > 6.0f) continue;

        float pX1 = cx - halfW;
        float pX2 = cx + halfW;
        bool overlapsLanding = false;
        for (int i = 0; i < (int)tl.size(); i++) {
            if (tl[i].landable && tl[i].multiplier > 1) {
                if (tl[i].x2 > pX1 && tl[i].x1 < pX2) {
                    overlapsLanding = true;
                    break;
                }
            }
        }
        if (overlapsLanding) continue;

        bodyX = cx;
        // On Titan the tanker hovers at a fixed world-y above every fog band;
        // elsewhere it hovers TANKER_HOVER_ALT above the ground.
        if (moonHasTitan(level)) baseY = TANKER_TITAN_Y;
        else baseY = terrainY - TANKER_HOVER_ALT;
        bodyY = baseY;
        originalBodyX = cx;
        portY = baseY + TANKER_HULL_H * 0.5f;
        driftX = ((rand() % 2) ? -1.0f : 1.0f) * TANKER_DRIFT_SPEED;
        phase = (float)(rand() % 628) / 100.0f;
        found = true;
    }

    if (!found) return;

    active = true;
}

bool Tanker::targeted() const
{
    return active && !docked && !leaving && !done;
}

static float drogueSway(float droguePhase, int port, float which)
{
    float a = droguePhase * TANKER_DROGUE_SWAY_SPEED + (port ? 2.1f : 0.0f);
    return sinf(a + which * 0.8f) * TANKER_DROGUE_SWAY;
}

float Tanker::drogueX() const
{
    return bodyX + drogueSway(droguePhase, 0, 0.0f);
}

float Tanker::drogueY() const
{
    return portY + TANKER_HOSE_LEN + drogueSway(droguePhase, 0, 1.0f);
}

bool Tanker::checkDock(const Ship& ship)
{
    if (!active || docked || leaving || done) return false;

    float rad = ship.rotation * PI / 180.0f;
    // The refuel probe sticks out of the module hull: at rotation 0 it points
    // up toward the underside drogue. Docking = probe tip inside the basket.
    float probeX = ship.posX + TANKER_NOZZLE_LEN * ship.scale * sinf(rad);
    float probeY = ship.posY - TANKER_NOZZLE_LEN * ship.scale * cosf(rad);

    if (fabsf(probeX - drogueX()) > TANKER_DOCK_TOL_X) return false;
    if (fabsf(probeY - drogueY()) > TANKER_DOCK_TOL_Y) return false;
    if (ship.velY > 0.09f) return false;
    if (ship.velY < -0.09f) return false;
    if (fabsf(ship.velX) > 0.14f) return false;
    return true;
}

void Tanker::beginDock(float shipVX, float shipVY)
{
    if (docked || leaving || done) return;
    docked = true;
    dockOffsetX = 0.0f;
    dockOffsetY = 0.0f;
    dockLockTimer = 0.0f;
    dockBreakTimer = 0.0f;
    fuelFlowing = false;
    // Keep a tiny residual velocity from the approach so the first offset
    // nudge reflects the entry angle.
    dockOffsetX += shipVX * 2.0f;
    dockOffsetY += shipVY * 2.0f;
}

void Tanker::move(float dt)
{
    bodyX += driftX * dt;
    if (bodyX < originalBodyX - TANKER_DRIFT_RANGE ||
        bodyX > originalBodyX + TANKER_DRIFT_RANGE) {
        driftX = -driftX;
    }
    phase += TANKER_BOB_SPEED * dt;
    droguePhase += TANKER_DROGUE_SWAY_SPEED * dt;
    bodyY = baseY + sinf(phase) * TANKER_BOB_AMP;
    portY = bodyY + TANKER_HULL_H * 0.5f;
}

bool Tanker::update(float dt, Ship& ship)
{
    if (!active || done) return false;

    if (docked) {
        move(dt);
        float sc = ship.scale;

        // Docking mini-game: the joystick nudges the ship's offset relative to
        // the drogue. The real drogue is a funnel: while the probe is threaded
        // into the cone mouth the tapered walls pull it home (centering), so
        // the player only needs to seat the probe — not fight a twitchy hold.
        // Nudge is deliberately low-sensitivity so a slight stick drift does
        // not drag the probe out of the cone.
        float nudgeSpeed = 9.0f * dt; // world units per second of joystick deflection
        dockOffsetX += ship.velX * nudgeSpeed;
        dockOffsetY += ship.velY * nudgeSpeed;
        // Funnel centering: strong pull while seated inside the cone, a firm
        // recovery spring outside so the probe is always pulled back toward
        // the mouth rather than drifting off.
        bool seated = fabsf(dockOffsetX) <= TANKER_DOCK_TOL_X &&
                      fabsf(dockOffsetY) <= TANKER_DOCK_TOL_Y;
        float pull = seated ? TANKER_CONE_GUIDE : (TANKER_CONE_GUIDE * 0.3f);
        dockOffsetX -= dockOffsetX * pull * dt;
        dockOffsetY -= dockOffsetY * pull * dt;

        // Clamp to a reasonable control range.
        if (dockOffsetX > 18.0f) dockOffsetX = 18.0f;
        if (dockOffsetX < -18.0f) dockOffsetX = -18.0f;
        if (dockOffsetY > 14.0f) dockOffsetY = 14.0f;
        if (dockOffsetY < -14.0f) dockOffsetY = -14.0f;

        bool aligned = fabsf(dockOffsetX) <= TANKER_DOCK_TOL_X &&
                       fabsf(dockOffsetY) <= TANKER_DOCK_TOL_Y;

        if (!aligned) {
            dockLockTimer = 0.0f;
            dockBreakTimer += dt;
            if (dockBreakTimer >= TANKER_DOCK_BREAK_TIME) {
                breakAway(ship);
                return false;
            }
        } else {
            dockBreakTimer = 0.0f;
            dockLockTimer += dt;
            if (dockLockTimer >= TANKER_DOCK_LOCK_TIME) {
                dockLockTimer = TANKER_DOCK_LOCK_TIME;
                fuelFlowing = true;
            }
        }

        // The module hangs upright off the underside hose basket (probe up),
        // riding the drogue's sway plus the player's fine offset.
        ship.posX = drogueX() + dockOffsetX;
        ship.posY = drogueY() + TANKER_NOZZLE_LEN * sc + dockOffsetY;
        ship.rotation = 0.0f;
        ship.targetRotation = 0.0f;
        ship.velX = 0;
        ship.velY = 0;

        if (fuelFlowing) {
            ship.fuel += TANKER_REFUEL_RATE * dt;
            if (ship.fuel > FUEL_MAX) ship.fuel = FUEL_MAX;
            if (ship.fuel >= FUEL_MAX) {
                ship.velX = 0;
                ship.velY = 0;
                docked = false;
                leaving = true;
                fuelFlowing = false;
                return true;
            }
        }
        return false;
    }

    if (leaving) {
        bodyX += TANKER_LEAVE_SPEED * dt;
        bodyY -= TANKER_LEAVE_SPEED * 0.6f * dt;
        droguePhase += TANKER_DROGUE_SWAY_SPEED * dt;
        portY = bodyY + TANKER_HULL_H * 0.5f;
        if (bodyX > originalBodyX + TANKER_LEAVE_DIST || bodyY < 40.0f) {
            done = true;
            leaving = false;
        }
        return false;
    }

    move(dt);
    return false;
}

void Tanker::breakAway(Ship& ship)
{
    if (!docked) return;
    docked = false;
    fuelFlowing = false;
    dockLockTimer = 0.0f;
    dockBreakTimer = 0.0f;
    // Eject the module in the direction of the current offset so it visibly
    // leaves the basket. The tanker stays on station for a reconnect.
    ship.velX = (dockOffsetX > 0.0f ? 1.0f : -1.0f) * 0.04f;
    ship.velY = 0.05f;
    ship.posX += dockOffsetX * 0.3f;
    ship.posY += 1.5f;
    dockOffsetX = 0.0f;
    dockOffsetY = 0.0f;
}

bool Tanker::hitsHull(float shipX, float shipY) const
{
    if (!active || docked || done) return false;
    float halfW = TANKER_HULL_W * 0.5f + TANKER_HULL_MARGIN;
    float balloonHalfH = 5.5f + TANKER_HULL_MARGIN * 0.5f;
    return fabsf(shipX - bodyX) < halfW && fabsf(shipY - bodyY) < balloonHalfH;
}

void Tanker::destroy()
{
    active = false;
    docked = false;
    leaving = false;
    done = true;
    fuelFlowing = false;
    dockLockTimer = 0.0f;
    dockBreakTimer = 0.0f;
}

static int engineFlicker(int counter, int n)
{
    return 90 + ((counter * 37 + n * 91) % 120);
}

void Tanker::drawDrogue(Renderer &r, float x, float y, float s)
{
    // Small guide marker at the general view's scale: a tiny upward triangle
    // whose bright apex is the align point the probe tip must reach.
    float a = 2.4f * s;
    float h = 2.8f * s;
    r.line(x, y, x - a, y + h);
    r.line(x, y, x + a, y + h);
    r.line(x - a, y + h, x + a, y + h);
    r.pixelShade(x, y, 255);
}

void Tanker::drawDroguePip(Renderer &r, float x, float y, float s)
{
    // Receiving basket = an inverted truncated cone. The narrow seat (top) is
    // the alignment point (x,y) the probe tip must reach; it flares down to
    // the wide mouth that faces the incoming probe from below.
    float mouth = 4.2f * s;    // wide mouth half-width (bottom)
    float len = 4.8f * s;      // distance down to the mouth
    float seat = 1.1f * s;     // narrow seat half-width (top)
    float my = y + len;        // mouth centre

    // cone walls (narrow seat up, wide mouth down)
    r.line(x - seat, y, x - mouth, my);
    r.line(x + seat, y, x + mouth, my);

    // mouth drawn as an opening ellipse so it reads as a hollow cone mouth
    int mx0 = (int)roundf(x - mouth), mx1 = (int)roundf(x + mouth);
    for (int px = mx0; px <= mx1; px++) {
        float t = (float)(px - mx0) / (mx1 - mx0);
        float ry = my - 0.35f * mouth * sinf(t * 3.14159265f);
        r.pixelShade((float)px, ry, 240);
    }

    // seat collar: the receiving lip the probe tip rests in
    r.line(x - seat, y, x + seat, y);

    // a supporting ring part-way down the cone
    float hw = seat + (mouth - seat) * 0.55f;
    float hy = y + len * 0.55f;
    r.line(x - hw, hy, x + hw, hy);

    // soft shading on the inner walls for a hint of depth
    r.lineShade(x - seat, y, x - mouth * 0.6f, my - 0.3f * mouth, 130);
    r.lineShade(x + seat, y, x + mouth * 0.6f, my - 0.3f * mouth, 130);

    // bright alignment point: the seat centre the probe tip must hit
    r.pixelShade(x, y, 255);
}

static void drawHose(Renderer &r, float x0, float y0, float x1, float y1, float s)
{
    // Flexible hose: a short polyline from the hull port to the drogue basket.
    // Small sinusoidal bends perpendicular to the straight line keep the
    // endpoints fixed while making it read as a hose instead of a ray.
    int segs = 5;
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    float nx = 0.0f, ny = 0.0f;
    if (len > 1e-3f) { nx = -dy / len; ny = dx / len; }
    float px = x0, py = y0;
    for (int i = 1; i <= segs; i++) {
        float t = (float)i / segs;
        float bend = sinf(t * 3.14159265f) * 1.4f * s;
        float cx = x0 + dx * t + nx * bend;
        float cy = y0 + dy * t + ny * bend;
        r.line(px, py, cx, cy);
        px = cx;
        py = cy;
    }
}

void Tanker::draw(Renderer &r, float viewX, float viewY, float viewScale, int counter,
                  const Ship& ship) const
{
    (void)ship;
    if (!active || done) return;

    // In the normal/zoomed-out view the zeppelin is drawn bigger so it reads
    // as a mothership, but the physical hitbox stays unchanged.
    float drawScale = (viewScale < 1.0f) ? viewScale * 1.6f : viewScale;
    float s = viewScale; // drogue/hose/gondola use viewScale; balloon uses drawScale
    float x0 = bodyX * s + viewX;
    float y0 = bodyY * s + viewY;

    // Zeppelin airship: an elongated balloon body with a small gondola
    // underneath, rear tail fins, a pulsing top beacon and a tail engine glow.
    float RX = 13.0f * drawScale;
    float RY = 5.5f * drawScale;
    int irx = (int)roundf(RX), iry = (int)roundf(RY);
    for (int yy = -iry; yy <= iry; yy++) {
        float fy = (float)yy / (float)iry;
        float q = 1.0f - fy * fy;
        if (q <= 0.0f) continue;
        int hw = (int)roundf(irx * sqrtf(q));
        if (hw > 0)
            r.lineShade(x0 - hw, y0 + yy, x0 + hw, y0 + yy,
                        120 + (int)(40.0f * (1.0f - fabsf(fy))));
    }
    for (int yy = -iry; yy <= iry; yy++) {
        float fy = (float)yy / (float)iry;
        float q = 1.0f - fy * fy;
        if (q <= 0.0f) continue;
        int hw = (int)roundf(irx * sqrtf(q));
        r.pixelShade(x0 - hw, y0 + yy, 255);
        r.pixelShade(x0 + hw, y0 + yy, 255);
    }
    int hly = (int)roundf(RY * 0.7f);
    for (int px = x0 - (int)roundf(RX * 0.6f); px <= x0 + (int)roundf(RX * 0.6f); px++)
        r.pixelShade((float)px, y0 - hly, 200);
    r.line(x0 - (int)roundf(RX * 0.8f), y0, x0 + (int)roundf(RX * 0.8f), y0);

    float gx = x0 - 1.0f * s, gy = y0 + RY;
    r.line(gx - 3.0f * s, gy, gx + 3.0f * s, gy);
    r.line(gx - 3.0f * s, gy, gx - 3.0f * s, gy + 2.2f * s);
    r.line(gx + 3.0f * s, gy, gx + 3.0f * s, gy + 2.2f * s);
    r.line(gx - 3.0f * s, gy + 2.2f * s, gx + 3.0f * s, gy + 2.2f * s);
    r.line(gx - 1.0f * s, gy + 1.0f * s, gx + 1.2f * s, gy + 1.0f * s);
    r.pixelShade(gx, gy + 1.2f * s, 230);
    r.line(gx - 2.0f * s, gy, gx - 2.0f * s, y0 + RY * 0.6f);
    r.line(gx + 2.0f * s, gy, gx + 2.0f * s, y0 + RY * 0.6f);

    float tail = x0 + RX - 1.0f * s;
    r.line(tail, y0 - 1.0f * s, tail + 4.5f * s, y0 - 4.0f * s);
    r.line(tail, y0 + 1.0f * s, tail + 4.5f * s, y0 + 4.0f * s);
    r.line(tail, y0 - 1.0f * s, tail + 4.5f * s, y0);
    r.line(tail, y0 + 1.0f * s, tail + 4.5f * s, y0);

    r.line(x0 + 4.0f * s, y0 - RY, x0 + 4.0f * s, y0 - RY - 2.5f * s);
    if ((counter / 25) & 1) r.pixelShade(x0 + 4.0f * s, y0 - RY - 2.5f * s, 230);
    else r.pixelShade(x0 + 4.0f * s, y0 - RY - 2.5f * s, 80);

    int fl = engineFlicker(counter, 3);
    r.pixelShade(x0 + RX, y0, 200 + fl / 4);
    r.pixelShade(x0 + RX + 1.0f * s, y0 - 1.0f * s, fl);
    r.pixelShade(x0 + RX + 1.0f * s, y0 + 1.0f * s, fl);

    // Refueling hose (probe-and-drogue): a flexible polyline runs from the
    // underside hull port down to the drogue basket. The drogue's back glows
    // brighter while the connection is live.
    float hs = 160.0f;
    if (docked) hs = 240.0f;
    drawHose(r, x0 + 2.0f * s, portY * s + viewY,
             drogueX() * s + viewX, drogueY() * s + viewY, s);
    r.pixelShade((drogueX() - 1.0f) * s + viewX, drogueY() * s + viewY, (int)hs);
    r.pixelShade(drogueX() * s + viewX + 1.0f * s, (drogueY() + 1.0f) * s + viewY, (int)(hs * 0.6f));

    drawDrogue(r, drogueX() * s + viewX, drogueY() * s + viewY, s);

    if (docked && fuelFlowing) {
        // Fuel flowing through the connected hose: bright drops running toward
        // the module's probe (which sits in the drogue basket).
        float hx0 = x0 + 2.0f * s;
        float hy0 = portY * s + viewY;
        float hx1 = drogueX() * s + viewX;
        float hy1 = drogueY() * s + viewY;
        int dropSteps = 5;
        for (int i = 0; i < dropSteps; i++) {
            float t = (float)(i + 1) / (dropSteps + 1);
            float animT = fmodf(t + (float)counter * 0.03f, 1.0f);
            float dropX = hx0 + (hx1 - hx0) * animT;
            float dropY = hy0 + (hy1 - hy0) * animT;
            float j = ((float)(i % 3) - 1.0f) * 1.5f * s;
            int b = (int)(210.0f * (1.0f - animT * 0.5f));
            if (b > 0) r.pixelShade(dropX + j, dropY, b);
        }
    }
}
