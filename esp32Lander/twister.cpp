#include <cmath>
#include <cstdlib>
#include "twister.h"
#include "moons.h"

Twister::Twister()
    : level_(1), enabled_(false), t_(0), cx_(400), drift_(1),
      strength_(1.0f), swirl_(1), phase_(0),
      escapeCooldown_(0), holdT_(0), escapeTicks_(0), tumbleDeg_(0),
      capOff_(0), swirlAngle_(0),
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

void Twister::reset(int level, const Terrain &t)
{
    (void)t;
    level_ = level;
    enabled_ = moonHasTwister(level);
    t_ = 0;
    escapeCooldown_ = 0;
    holdT_ = 0;
    escapeTicks_ = 0;
    tumbleDeg_ = 0;
    capOff_ = 0;
    swirlAngle_ = 0;
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

bool Twister::apply(Ship &s, const Terrain &t)
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

    // Escape: the player must actually break the grip — sustained outward
    // radial motion while pushing with enough outward thrust. While the ship
    // "fights" (radial thrust above the threshold) the funnel hold below is
    // switched off, so the burn really moves it outward; once it climbs past
    // the escape velocity for enough ticks it is flung out and the grip stays
    // off until it physically leaves the vortex radius.
    float rad = s.rotation * PI / 180.0f;
    float hx = sinf(rad), hy = -cosf(rad);
    float thrustOut = THRUST_ACCEL * s.thrustBuild * (hx * ux + hy * uy);
    if (thrustOut < 0.0f) thrustOut = 0.0f;
    float rv = s.velX * ux + s.velY * uy;          // + outward
    bool fighting = thrustOut > TWISTER_ESCAPE_THRUST * strength_;
    if (fighting && rv > TWISTER_ESCAPE_VEL) escapeTicks_++;
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
    // always stays inside the drawn vortex.
    bool firstCapture = (holdT_ <= 0.0f);
    holdT_ += 1.0f;
    float ease = (holdT_ < TWISTER_CAPTURE_RAMP)
                     ? holdT_ / TWISTER_CAPTURE_RAMP : 1.0f;
    if (firstCapture) {
        capOff_ = s.posX - cx_;
        if (fabsf(capOff_) < 1.0f) capOff_ = (capOff_ < 0.0f) ? -1.0f : 1.0f;
        swirlAngle_ = 0.0f;
    }

    float h = cy - s.posY;                     // height above the ground base
    if (h < 0.0f) h = 0.0f;
    if (h > TWISTER_HEIGHT) h = TWISTER_HEIGHT;
    float coneR = TWISTER_BASE_HALF +
                  (TWISTER_TOP_HALF - TWISTER_BASE_HALF) * (h / TWISTER_HEIGHT);

    float omega = TWISTER_SPIRAL_RATE * (PI / 180.0f) * strength_;
    swirlAngle_ += (float)swirl_ * omega * GAME_DT;

    float amp = fabsf(capOff_) + (coneR - fabsf(capOff_)) * ease;
    if (amp > TWISTER_RADIUS) amp = TWISTER_RADIUS;
    if (amp < TWISTER_BASE_HALF) amp = TWISTER_BASE_HALF;
    float dir = (capOff_ < 0.0f) ? -1.0f : 1.0f;

    s.posX = cx_ + dir * amp * cosf(swirlAngle_);
    s.posY = cy - h;

    // Velocity matching the weave (keeps the HUD alive and carries momentum
    // into an escape). Positive velY is downward.
    s.velX = -dir * amp * omega * sinf(swirlAngle_) * GAME_DT;
    s.velY = TWISTER_DESCENT * strength_ * GAME_DT;

    // Nose tumbles continuously with a wandering jitter (270-360+ degrees).
    tumbleDeg_ += (float)swirl_ * TWISTER_TUMBLE_RATE * strength_ * GAME_DT;
    float jitter = TWISTER_TUMBLE_JITTER * strength_ *
                   sinf(t_ * TWISTER_TUMBLE_WAVE + phase_);
    s.rotation = tumbleDeg_ + jitter;
    s.targetRotation = s.rotation;

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

        // Swirling dust bands across alternate rows.
        if (((int)(wy / (TWISTER_BAND_STEP * 2.0f)) & 1) == 0) {
            float rot = t_ * TWISTER_SWIRL_SPEED + tt * 4.0f + phase_;
            for (int xx = -half; xx <= half; xx++) {
                float depth = (float)fabs(xx) / (float)half;
                int b = (int)(45.0f + 165.0f * depth);
                float yoff = sinf(rot + (float)xx * 0.5f) * 0.5f;
                r.pixelShade(cxx + (float)xx, sy + yoff, b);
            }
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
