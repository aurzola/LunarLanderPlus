#include <cmath>
#include <cstdlib>
#include "twister.h"
#include "moons.h"

Twister::Twister()
    : level_(1), enabled_(false), t_(0), cx_(400), drift_(1),
      strength_(1.0f), swirl_(1), phase_(0),
      escapeCooldown_(0), holdT_(0), escapeTicks_(0),
      capOff_(0), swirlAngle_(0), weavePrevX_(0),
      captured_(false), escaped_(false)
{
}

float Twister::terrainYAt(const Terrain &t, float x, float fallback)
{
    const std::vector<TerrainLine> &tl = t.getLines();
    for (int i = 0; i < (int)tl.size(); i++) {
        const TerrainLine &l = tl[i];
        if (x >= l.x1 && x <= l.x2 && l.x2 != l.x1) {
            float tt = (x - l.x1) / (l.x2 - l.x1);
            return l.y1 + (l.y2 - l.y1) * tt;
        }
    }
    return fallback;
}

// Triangle wave in [-1,1] starting at +1 (the entry side): it sweeps down to -1
// over one half period, back up to +1 over the next, etc. Used to make the
// captured ship zig-zag between the two walls of the funnel cone.
static float triWave(float u)
{
    float p = 2.0f * PI;
    float x = fmodf(u, p);
    if (x < 0.0f) x += p;
    if (x < PI) return 1.0f - 2.0f * x / PI;
    return -1.0f + 2.0f * (x - PI) / PI;
}

void Twister::reset(int level, const Terrain &t)
{
    (void)t;
    level_ = level;
    enabled_ = moonHasTwister(level);
    t_ = 0;
    escapeCooldown_ = 0;
    holdT_ = 0;
    escapeTicks_ = 0;
    capOff_ = 0;
    swirlAngle_ = 0;
    weavePrevX_ = 0;
    captured_ = false;
    escaped_ = false;
    if (!enabled_) return;

    // Match the wander bounds used by update(): cx stays in [40, 760].
    cx_ = 60.0f + (float)(rand() % 640); // -> [60, 700]
    drift_ = (rand() % 2) ? 1.0f : -1.0f;
    strength_ = TWISTER_STRENGTH_MIN +
                ((float)(rand() % 1000) / 1000.0f) *
                (TWISTER_STRENGTH_MAX - TWISTER_STRENGTH_MIN);
    swirl_ = (rand() % 2) ? 1 : -1;
    phase_ = (float)(rand() % 1000) / 1000.0f * 6.2831853f;
}

void Twister::update(float dt)
{
    if (!enabled_) return;
    t_ += dt;
    // Drift across the world, bouncing at the edges.
    cx_ += drift_ * TWISTER_DRIFT_SPEED * dt;
    if (cx_ < 40.0f) { cx_ = 40.0f; drift_ = -drift_; }
    if (cx_ > 760.0f) { cx_ = 760.0f; drift_ = -drift_; }
}

bool Twister::apply(Ship &s, const Terrain &t, float stickDeg)
{
    if (!enabled_) return false;

    if (escapeCooldown_ > 0) {
        escapeCooldown_ -= GAME_DT;
        if (escapeCooldown_ < 0) escapeCooldown_ = 0;
        captured_ = false;
        escaped_ = false;
        holdT_ = 0;
        return false;
    }

    // Vortex center is the funnel base sitting on the ground.
    float cy = terrainYAt(t, cx_, 480.0f);
    float dx = s.posX - cx_;
    float dy = s.posY - cy;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist >= TWISTER_RADIUS || dist < 0.01f) {
        captured_ = false;
        escaped_ = false;
        holdT_ = 0;
        escapeTicks_ = 0;
        return false;
    }

    float ux = dx / dist, uy = dy / dist;    // outward unit from the core
    float tx = -uy * (float)swirl_, ty = ux * (float)swirl_; // tangent unit

    // Height above the ground base: 0 at the ground, TWISTER_HEIGHT at the
    // top. depth goes 0 at the top -> 1 at the ground and is what makes the
    // vortex progressively harder to escape the deeper the ship is.
    float h = cy - s.posY;
    if (h < 0.0f) h = 0.0f;
    if (h > TWISTER_HEIGHT) h = TWISTER_HEIGHT;
    float depth = 1.0f - h / TWISTER_HEIGHT;

    // Escape: the player must actually break the grip — sustained outward
    // radial motion while pushing with enough outward thrust. Both the radial
    // thrust that counts as "fighting" and the outward speed needed scale with
    // depth: near the top a burst of power breaks it, near the ground the
    // funnel holds the ship until it smashes. While the ship "fights" the
    // funnel hold below is switched off, so the burn really moves it outward;
    // once it climbs past the escape velocity for enough ticks it is flung out
    // and the grip stays off until it physically leaves the vortex radius.
    float rad = s.rotation * PI / 180.0f;
    float hx = sinf(rad), hy = -cosf(rad);
    float thrustOut = THRUST_ACCEL * s.thrustBuild * (hx * ux + hy * uy);
    if (thrustOut < 0.0f) thrustOut = 0.0f;
    float rv = s.velX * ux + s.velY * uy;          // + outward
    float escThr = TWISTER_ESCAPE_THRUST *
                   (1.0f + TWISTER_ESCAPE_DEPTH_THRUST * depth);
    float escVel = TWISTER_ESCAPE_VEL *
                   (1.0f + TWISTER_ESCAPE_DEPTH_VEL * depth);
    bool fighting = thrustOut > escThr;
    if (fighting && rv > escVel) escapeTicks_++;
    else escapeTicks_ = 0;
    if (escapeTicks_ >= TWISTER_ESCAPE_TICKS) {
        s.velX += ux * TWISTER_FLING * strength_;
        s.velY += uy * TWISTER_FLING * strength_;
        s.rotation += (float)swirl_ * TWISTER_SPIN_KICK * strength_;
        float flingV = TWISTER_FLING * strength_ + 0.15f;
        escapeCooldown_ = (dist + 40.0f) / flingV;
        captured_ = false;
        escaped_ = true;
        holdT_ = 0;
        escapeTicks_ = 0;
        return false;
    }

    if (fighting) {
        // Grip relaxed: the longer the player pushes, the weaker the swirl
        // left on the ship, so breaking free is gradual and the burn itself
        // carries it outward. No funnel hold while fighting.
        if (holdT_ > 0) holdT_ -= 1.0f;
        float grip = TWISTER_HOLD_GAIN * (holdT_ / TWISTER_HOLD_RAMP);
        if (grip > 0.0f) {
            float vTan = (float)swirl_ * TWISTER_ORBIT_SPEED * dist;
            if (vTan > TWISTER_ORBIT_MAX) vTan = TWISTER_ORBIT_MAX;
            if (vTan < -TWISTER_ORBIT_MAX) vTan = -TWISTER_ORBIT_MAX;
            float tv = s.velX * tx + s.velY * ty;
            s.velX += (vTan - tv) * tx * grip;
            s.velY += (vTan - tv) * ty * grip;
        }
        captured_ = false;
        escaped_ = false;
        return false;
    }

    // Full capture: the ship rides the funnel. The funnel is a cone that tapers
    // to a thin point on the ground; the ship is eased onto the wall, then
    // weaves between the walls (horizontal spiral around the core) while it is
    // sunk, so it descends in a spiral that follows the funnel exactly and
    // always stays inside the drawn vortex, barely poking outside it.
    bool firstCapture = (holdT_ <= 0.0f);
    holdT_ += 1.0f;
    float ease = (holdT_ < TWISTER_CAPTURE_RAMP)
                     ? holdT_ / TWISTER_CAPTURE_RAMP : 1.0f;
    if (firstCapture) {
        capOff_ = s.posX - cx_;
        if (fabsf(capOff_) < 1.0f) capOff_ = (capOff_ < 0.0f) ? -1.0f : 1.0f;
        swirlAngle_ = 0.0f;
        weavePrevX_ = s.posX;
    }

    float coneR = TWISTER_BASE_HALF +
                  (TWISTER_TOP_HALF - TWISTER_BASE_HALF) * (h / TWISTER_HEIGHT);

    float omega = TWISTER_SPIRAL_RATE * (PI / 180.0f) * strength_;
    swirlAngle_ += (float)swirl_ * omega * GAME_DT;

    // Zig-zag across the funnel: a triangle wave sends the ship back and forth
    // between the two cone walls, and because the cone tapers toward the ground
    // the zig-zag closes as the ship descends. A small overshoot lets it barely
    // poke outside the wall (never out of the vortex radius).
    float poke = 1.0f + TWISTER_EDGE_POKE *
                        sinf(swirlAngle_ * 2.0f + phase_);
    float target = coneR * poke;
    if (target > TWISTER_RADIUS) target = TWISTER_RADIUS;
    if (target < TWISTER_BASE_HALF) target = TWISTER_BASE_HALF;
    float amp = fabsf(capOff_) + (target - fabsf(capOff_)) * ease;
    float dir = (capOff_ < 0.0f) ? -1.0f : 1.0f;

    s.posX = cx_ + dir * amp * triWave(swirlAngle_);
    s.posY = cy - h;

    // Per-tick sweep velocity (same convention as the descent velY).
    float sweep = s.posX - weavePrevX_;
    if (sweep > TWISTER_ORBIT_MAX) sweep = TWISTER_ORBIT_MAX;
    if (sweep < -TWISTER_ORBIT_MAX) sweep = -TWISTER_ORBIT_MAX;
    s.velX = sweep;
    weavePrevX_ = s.posX;
    s.velY = TWISTER_DESCENT * strength_ * GAME_DT;

    // The nose rocks between +/-60 degrees; the joystick tilts it further but
    // never reaches the full +/-90 authority (hard-capped well below).
    float wobble = TWISTER_WOBBLE_RANGE *
                   sinf(t_ * TWISTER_WOBBLE_RATE * strength_ + phase_);
    float rot = wobble + stickDeg * TWISTER_STICK_GAIN;
    if (rot > TWISTER_WOBBLE_MAX) rot = TWISTER_WOBBLE_MAX;
    if (rot < -TWISTER_WOBBLE_MAX) rot = -TWISTER_WOBBLE_MAX;
    s.rotation = rot;
    s.targetRotation = rot;

    captured_ = true;
    escaped_ = false;
    return true;
}

void Twister::draw(Renderer &r, const Terrain &t,
                   float viewX, float viewY, float viewScale) const
{
    if (!enabled_) return;

    float gy = terrainYAt(t, cx_, 480.0f);
    float topY = gy - TWISTER_HEIGHT;

    float sx0 = (cx_ - TWISTER_RADIUS) * viewScale + viewX;
    float sx1 = (cx_ + TWISTER_RADIUS) * viewScale + viewX;
    float syTop = topY * viewScale + viewY;
    float syBot = gy * viewScale + viewY;
    if (sx1 < -60.0f || sx0 > SCREEN_W + 60.0f) return;
    if (syBot < -60.0f || syTop > SCREEN_H + 60.0f) return;

    float sway = sinf(t_ * TWISTER_SWAY_SPEED + phase_) * TWISTER_SWAY_AMP;

    // Original funnel: oscillating cone (base TWISTER_BASE_HALF -> top
    // TWISTER_TOP_HALF), sinusoidal sway growing upward plus a micro-sway;
    // horizontal dust bands across alternate rows give the swirling texture.
    for (float wy = gy; wy >= topY; wy -= TWISTER_BAND_STEP) {
        float tt = (gy - wy) / TWISTER_HEIGHT; // 0 at base, 1 at top
        float halfW = TWISTER_BASE_HALF +
                      (TWISTER_TOP_HALF - TWISTER_BASE_HALF) * tt;
        float cxs = cx_ + sway * tt +
                    sinf(t_ * TWISTER_SWIRL_SPEED * 0.7f + tt * 3.0f + phase_) *
                    TWISTER_SWAY_AMP * 0.5f * tt;
        float sy = wy * viewScale + viewY;
        if (sy < -30.0f || sy > SCREEN_H + 30.0f) continue;

        float cxx = cxs * viewScale + viewX;
        int half = (int)ceilf(halfW * viewScale);
        if (half < 1) half = 1;

        // Continuous horizontal dust lines across each funnel row: no dots or
        // bright edges on the extremes, just clean horizontal lines whose
        // length grows with the cone (short at the ground -> long at the top).
        // A gentle along-row shading hints the swirling spiral.
        float rot = t_ * TWISTER_SWIRL_SPEED + tt * 4.0f + phase_;
        for (int xx = -half; xx <= half; xx++) {
            int b = (int)(TWISTER_BAND_BRIGHT +
                          TWISTER_BAND_SWIRL * sinf(rot + (float)xx * 0.25f));
            if (b > (int)TWISTER_BAND_MAX) b = (int)TWISTER_BAND_MAX;
            r.pixelShade(cxx + (float)xx, sy, b);
        }
    }

    // Swirling dust cloud at the base (thin tip).
    for (int k = 0; k < 6; k++) {
        float ang = t_ * TWISTER_SWIRL_SPEED + phase_ + (float)k * 1.0472f;
        float rr = TWISTER_BASE_HALF + (float)k * 1.6f;
        float px = cx_ + cosf(ang) * rr * 1.6f;
        float py = gy - 1.0f + fabsf(sinf(ang)) * 2.5f;
        float b = 120.0f + 40.0f * (float)sinf(t_ * 6.0f + (float)k);
        if (b > 160) b = 160;
        r.pixelShade(px * viewScale + viewX, py * viewScale + viewY, b);
    }

    // Debris orbiting on the funnel wall: makes the circular swirl visible.
    for (int k = 0; k < TWISTER_ORBIT_COUNT; k++) {
        float tt = 0.1f + (float)k * (0.9f / (float)(TWISTER_ORBIT_COUNT - 1));
        float halfW = TWISTER_BASE_HALF +
                      (TWISTER_TOP_HALF - TWISTER_BASE_HALF) * tt;
        float ang = t_ * TWISTER_SWIRL_SPEED * (1.0f + 0.3f * tt) +
                    phase_ + (float)k * 0.8f;
        float px = cx_ + cosf(ang) * (halfW + 3.0f);
        float py = gy - tt * TWISTER_HEIGHT + sinf(ang * 1.7f) * 3.0f;
        int b = 120 + (int)(110.0f * (0.5f + 0.5f * sinf(ang * 2.0f)));
        if (b > 230) b = 230;
        r.pixelShade(px * viewScale + viewX, py * viewScale + viewY, b);
    }
}
