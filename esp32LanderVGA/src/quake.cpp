#include <cstdlib>
#include <cmath>
#include "quake.h"
#include "config.h"
#include "moons.h"

static float prand(int seed)
{
    unsigned int s = (unsigned int)seed * 1103515245u + 12345u;
    return (float)(s & 0x7fffffffu) / (float)0x7fffffff;
}

Quake::Quake()
    : enabled_(false), phase_(IDLE), timer_(0), rumbleT_(0), rearmT_(0),
      rupturedZone_(-1), targetX_(0), targetGroundY_(0),
      rupturedX1_(1), rupturedX2_(-1), shake_(0), justStruck_(false),
      justRumbled_(false) {}

void Quake::reset(int level, const Terrain &t, const Ship &s)
{
    (void)t; (void)s;
    enabled_ = moonHasQuakes(level);
    phase_ = IDLE;
    timer_ = QUAKE_START_TIME_MIN +
             (float)(rand() % (int)((QUAKE_START_TIME_MAX - QUAKE_START_TIME_MIN) * 100.0f + 1)) / 100.0f;
    rumbleT_ = 0;
    rearmT_ = 0;
    rupturedZone_ = -1;
    targetX_ = 0;
    targetGroundY_ = 0;
    rupturedX1_ = 1;
    rupturedX2_ = -1;
    shake_ = 0;
    justStruck_ = false;
    justRumbled_ = false;
    dust_.clear();
}

static void pickStrikeTarget(float &tx, const Ship &s)
{
    float jitter = ((float)(rand() % 2000) / 1000.0f - 1.0f) * QUAKE_STRIKE_JITTER;
    tx = s.posX + jitter;
    if (tx < 20.0f) tx = 20.0f;
}

static void spawnDustAt(std::vector<Quake::Dust> &dust, float x, float groundY,
                        int count, float rangeScale, float vxScale, float vyScale,
                        float lifeScale)
{
    for (int i = 0; i < count; i++) {
        Quake::Dust d;
        d.x = x + ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * QUAKE_DUST_RANGE * rangeScale;
        d.y = groundY - ((float)rand() / (float)RAND_MAX) * QUAKE_DUST_HEIGHT;
        d.vx = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * 0.2f * vxScale;
        d.vy = -((float)rand() / (float)RAND_MAX) * 0.5f * vyScale;
        d.life = QUAKE_DUST_LIFE * lifeScale;
        d.maxLife = d.life;
        dust.push_back(d);
    }
}

void Quake::update(float dt, Terrain &t, const Ship &s, bool canStrike)
{
    if (!enabled_) return;

    justStruck_ = false;
    justRumbled_ = false;

    if (!canStrike) {
        // Not actively flying: let the shake and dust settle, but hold the
        // strike machine so a new quake never fires over a crash/landing
        // message (it re-arms on the next level anyway).
        shake_ *= 0.88f;
        if (shake_ < 0.05f) shake_ = 0;
        for (int i = 0; i < (int)dust_.size(); i++) {
            dust_[i].x += dust_[i].vx * (dt * 60.0f);
            dust_[i].y += dust_[i].vy * (dt * 60.0f);
            dust_[i].vy += 0.001f * (dt * 60.0f);
            dust_[i].life -= dt * 60.0f;
        }
        int d2 = (int)dust_.size() - 1;
        while (d2 >= 0) {
            if (dust_[d2].life <= 0) {
                dust_[d2] = dust_.back();
                dust_.pop_back();
            }
            d2--;
        }
        return;
    }

    if (phase_ == IDLE) {
        timer_ -= dt;
        if (timer_ <= 0) {
            // Strike follows the ship: whatever its altitude, the ground near
            // it buckles — a crack below the descent, or the very pad being
            // approached when the ship is close to the surface.
            pickStrikeTarget(targetX_, s);
            if (targetX_ > t.getWidth() - 20.0f) targetX_ = t.getWidth() - 20.0f;
            targetGroundY_ = t.yAt(targetX_, 500.0f);
            phase_ = RUMBLING;
            justRumbled_ = true;
            rumbleT_ = QUAKE_RUMBLE_TIME;
            shake_ = QUAKE_SHAKE_MAX;
            spawnDustAt(dust_, targetX_, targetGroundY_, QUAKE_DUST_COUNT, 1.0f, 1.0f, 1.0f, 1.0f);
        }
    } else if (phase_ == RUMBLING) {
        rumbleT_ -= dt;
        shake_ = QUAKE_SHAKE_MAX * (rumbleT_ / QUAKE_RUMBLE_TIME);
        if (shake_ < 0) shake_ = 0;
        if (rumbleT_ <= 0) {
            t.ruptureSurface(targetX_, QUAKE_SURFACE_HALF_W);
            rupturedX1_ = targetX_ - QUAKE_SURFACE_HALF_W;
            rupturedX2_ = targetX_ + QUAKE_SURFACE_HALF_W;
            rupturedZone_ = t.zoneOverlapping(rupturedX1_, rupturedX2_);
            phase_ = BROKEN;
            justStruck_ = true;
            rearmT_ = QUAKE_REARM_TIME;
            shake_ = QUAKE_SHAKE_MAX * 1.5f;
            spawnDustAt(dust_, targetX_, targetGroundY_, QUAKE_DUST_COUNT, 2.0f, 2.0f, 1.5f, 1.5f);
        }
    } else {
        // Brief display of the fresh crack, then re-arm for the next strike
        // so quakes also hit while the ship is low and descending.
        shake_ *= 0.88f;
        if (shake_ < 0.05f) shake_ = 0;
        rearmT_ -= dt;
        if (rearmT_ <= 0) {
            phase_ = IDLE;
            timer_ = QUAKE_NEXT_TIME_MIN +
                     (float)(rand() % (int)((QUAKE_NEXT_TIME_MAX - QUAKE_NEXT_TIME_MIN) * 100.0f + 1)) / 100.0f;
        }
    }

    float f = dt * 60.0f;
    for (int i = 0; i < (int)dust_.size(); i++) {
        dust_[i].x += dust_[i].vx * f;
        dust_[i].y += dust_[i].vy * f;
        dust_[i].vy += 0.001f * f;
        dust_[i].life -= f;
    }
    int d2 = (int)dust_.size() - 1;
    while (d2 >= 0) {
        if (dust_[d2].life <= 0) {
            dust_[d2] = dust_.back();
            dust_.pop_back();
        }
        d2--;
    }
}

void Quake::draw(Renderer &r, float viewX, float viewY, float viewScale)
{
    if (!enabled_ || phase_ == IDLE) return;

    for (int i = 0; i < (int)dust_.size(); i++) {
        const Dust &d = dust_[i];
        float sx = d.x * viewScale + viewX;
        float sy = d.y * viewScale + viewY;
        float t = d.life / d.maxLife;
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        int b = (int)(100.0f + 155.0f * t);
        if (b > 255) b = 255;
        r.pixelShade((int)sx, (int)sy, b);
        if (t > 0.3f) {
            r.pixelShade((int)sx + 1, (int)sy, b / 2);
            r.pixelShade((int)sx - 1, (int)sy, b / 2);
            r.pixelShade((int)sx, (int)sy + 1, b / 2);
        }
    }

    if (phase_ == RUMBLING && targetX_ > 0) {
        float sx = targetX_ * viewScale + viewX;
        float sy = targetGroundY_ * viewScale + viewY;
        int pulse = 4 + (int)(rumbleT_ * 6.0f);
        for (int i = 0; i < pulse; i++) {
            float dx = prand(i * 7919 + 1) * 12.0f - 6.0f;
            float dy = prand(i * 6271 + 2) * 8.0f - 4.0f;
            r.pixelShade((int)(sx + dx), (int)(sy + dy), 200);
        }
        float ck = 4.0f + 2.0f * prand(0);
        for (int i = 0; i < (int)ck; i++) {
            float cx = sx + prand(i * 4691 + 3) * 20.0f - 10.0f;
            float cy = sy + prand(i * 3371 + 4) * 10.0f - 5.0f;
            r.pixelShade((int)cx, (int)cy, 140);
        }
    }
}
