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
                   float viewX, float viewY, float viewScale,
                   bool zoomedIn) const
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

    const int M = 220;
    const int DIR = (swirl_ < 0) ? -1 : 1;
    const float TAU = 6.2831853f;

    // Pseudo-random in [0,1], deterministic per frame from an integer seed and
    // the animated phase, so the irregularity is stable but drifts over time.
    auto prand = [&](int k) -> float {
        float x = sinf((float)(k * 127 + 311) + phase_ * 3.0f) * 43758.5453f;
        return x - (float)(int)x;
    };

    auto axisX = [&](float tt) -> float {
        return cx_ + sinf(t_ * TWISTER_SWAY_SPEED + phase_ + tt * 1.7f) *
                     TWISTER_SWAY_AMP * (0.4f + 0.6f * tt);
    };
    float rotBase = t_ * 2.6f * (float)DIR;

    auto shadeSeg = [&](float x0, float y0, float x1, float y1, int b) {
        float dx = x1 - x0, dy = y1 - y0;
        float L = sqrtf(dx * dx + dy * dy);
        int n = (int)ceilf(L);
        if (n < 1) n = 1;
        for (int k = 0; k <= n; k++)
            r.pixelShade(x0 + dx * (float)k / (float)n,
                         y0 + dy * (float)k / (float)n, b);
    };

    int coilCount = zoomedIn ? 3 : 2;       // third spiral only in zoom-in phase
    int turns = zoomedIn ? 5 : 7;           // fewer turns in zoom so the 3 coils separate

    // Thin pig-tail / coil-spring: helical wires that coil down and taper to a
    // point near the ground. Wide at the top, tightening toward a thin tail at
    // the base. Spirals winding in parallel, broken at irregular gaps,
    // brightness fades toward the back of the coil so the 3D spin reads. The
    // third spiral (in zoom) is offset in phase for a denser, tied wind.
    for (int w = 0; w < coilCount; w++) {
        float aBase = rotBase + (float)w * (TAU / (float)coilCount);
        float px = 0.0f, py = 0.0f;
        bool open = false;
        for (int i = 0; i <= M; i++) {
            float tt = (float)i / (float)M;
            float ang = aBase + (float)DIR * tt * turns * TAU;
            float r = 4.0f + 30.0f * tt;            // thin tail -> wide top
            if (r < 1.0f) r = 1.0f;
            float jit = (prand(i + w * 17) - 0.5f) * 2.0f;
            float wx = axisX(tt) + cosf(ang) * r + jit;
            float wyy = gy - tt * TWISTER_HEIGHT;
            float sx = wx * viewScale + viewX;
            float sy = wyy * viewScale + viewY;
            if (prand(i + 300 + w * 29) > 0.78f) {       // irregular gap
                open = false;
                continue;
            }
            // Spiral shading: front of the coil bright, back dim.
            float front = 0.5f + 0.5f * cosf(ang);
            int b = (int)(70.0f + 165.0f * front * (0.4f + 0.6f * prand(i + 400 + w * 11)));
            if (b > 235) b = 235;
            if (b < 40) b = 40;
            if (open) shadeSeg(px, py, sx, sy, b);
            // Slightly thicker stroke: one extra dim pixel beside the line.
            if (open) shadeSeg(px + 1.0f, py, sx + 1.0f, sy, b * 2 / 3);
            px = sx; py = sy; open = true;
        }
    }

    // Sparse glow points near the top lip to hint where the coil starts.
    {
        float ttl = 0.96f;
        float cyL = (gy - ttl * TWISTER_HEIGHT) * viewScale + viewY;
        float cxL = axisX(ttl) * viewScale + viewX;
        float rt = (4.0f + 30.0f * ttl) * viewScale;
        for (int k = 0; k < 14; k++) {
            if (prand(k + 700) < 0.4f) continue;
            float a = rotBase * 0.3f + (float)k / 14.0f * TAU;
            int b = 140 + (int)(90.0f * prand(k + 800));
            if (b > 240) b = 240;
            r.pixelShade(cxL + cosf(a) * rt, cyL + sinf(a) * rt * 0.4f, b);
        }
    }

    // Rotating swirl dots at a few heights to cue the spin direction.
    for (int z = 0; z < 3; z++) {
        float tt = 0.15f + 0.35f * (float)z;
        float rw = (4.0f + 30.0f * tt);
        float syy = (gy - tt * TWISTER_HEIGHT) * viewScale + viewY;
        for (int k = 0; k < 6; k++) {
            if (prand(k + z * 13 + 900) < 0.3f) continue;
            float ang = rotBase * 1.8f + (float)k * 1.0472f + phase_;
            float wx = axisX(tt) + cosf(ang) * rw;
            int b = 120 + (int)(110.0f * prand(k + 1000));
            if (b > 240) b = 240;
            r.pixelShade(wx * viewScale + viewX, syy, b);
        }
    }

    // Small irregular dust skirt at the thin tail base.
    {
        float syy = (gy - 1.0f) * viewScale + viewY;
        float cxL = axisX(0.0f) * viewScale + viewX;
        for (int k = 0; k < 14; k++) {
            if (prand(k + 1100) < 0.3f) continue;
            float a = rotBase + phase_ + (float)k * 0.45f;
            float ex = cosf(a) * 6.0f * viewScale * 1.4f;
            float ey = sinf(a) * 2.0f * viewScale;
            int b = 95 + (int)(105.0f * prand(k + 1200));
            if (b > 205) b = 205;
            r.pixelShade(cxL + ex, syy + ey, b);
        }
    }
}
