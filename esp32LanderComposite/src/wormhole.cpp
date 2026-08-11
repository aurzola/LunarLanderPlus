#include <cmath>
#include "wormhole.h"
#include "ship.h"
#include "renderer.h"

namespace {
const float TAU = 6.2831853f;
const float SQUASH = 0.5f; // vertical squash: tilted accretion disc (elliptical, not circular)
}

Wormhole::Wormhole()
    : enabled_(false), phase_(WH_IDLE), t_(0), cx_(400), cy_(200),
      spin_(1), rot_(0), phaseSeed_(0), swallowed_(false), captured_(false),
      captureT_(0), startAng_(0), captureRad_(0), captureScale_(1)
{
}

void Wormhole::reset(float cx, float cy)
{
    enabled_ = true;
    phase_ = WH_EMERGING;
    t_ = 0;
    cx_ = cx;
    cy_ = cy;
    spin_ = (rand() % 2) ? 1 : -1;
    rot_ = (float)(rand() % 628) / 100.0f;
    phaseSeed_ = (float)(rand() % 1000) / 1000.0f * TAU;
    swallowed_ = false;
    captured_ = false;
    captureT_ = 0;
    startAng_ = 0;
    captureRad_ = 0;
    captureScale_ = 1;
}

void Wormhole::update(float dt)
{
    if (!enabled_) return;
    t_ += dt;

    switch (phase_) {
    case WH_EMERGING: {
        float fade = t_ / WORMHOLE_EMERGE_T;
        if (fade > 1.0f) fade = 1.0f;
        rot_ += (float)spin_ * WORMHOLE_SPIN * dt * fade; // spin-up
        if (t_ >= WORMHOLE_EMERGE_T) {
            phase_ = WH_ACTIVE;
            t_ = 0;
        }
        break;
    }
    case WH_ACTIVE:
        rot_ += (float)spin_ * WORMHOLE_SPIN * dt;
        // The field stays live for the whole level: no fade-out timer. It only
        // ends after a swallow (Game resets via wormholeJump/disable) or on a
        // level reset.
        break;
    case WH_SWALLOW:
        rot_ += (float)spin_ * WORMHOLE_SPIN * dt;
        if (t_ >= 0.15f) {
            phase_ = WH_DYING;
            t_ = 0;
        }
        break;
    case WH_DYING:
        rot_ += (float)spin_ * WORMHOLE_SPIN * dt;
        if (t_ >= WORMHOLE_DIE_T) {
            phase_ = WH_IDLE;
            enabled_ = false;
        }
        break;
    default:
        break;
    }
}

bool Wormhole::apply(Ship &s)
{
    if (!enabled_ || phase_ != WH_ACTIVE) return false;

    float dx = cx_ - s.posX;
    float dy = cy_ - s.posY;
    float d = sqrtf(dx * dx + dy * dy);
    if (d > WORMHOLE_GRAB_R) return false;

    if (!captured_) {
        // Already at the rim: swallow right away.
        if (d < WORMHOLE_SWALLOW_R) {
            swallowed_ = true;
            phase_ = WH_SWALLOW;
            t_ = 0;
            return true;
        }
        // Outer zone: pure radial pull toward the core, weaker the farther
        // out. Full thrust away from the center overpowers it, so the ship
        // stays free. Collisions stay on (return false).
        if (d >= WORMHOLE_CAPTURE_R) {
            float a = WORMHOLE_PULL_MAX * (1.0f - d / WORMHOLE_GRAB_R);
            if (d > 1.0f) {
                float ux = dx / d, uy = dy / d;
                s.velX += ux * a;
                s.velY += uy * a;
            }
            return false;
        }
        // Crossed into the capture zone: the ship is trapped from now on.
        // Snapshot the spiral start in the SQUASHED coordinate system so the
        // scripted path begins CONTINUOUSLY at the ship's actual position (the
        // old code used the ship->core angle with an unsquashed radius, which
        // teleported the ship to the mirrored point of the ellipse). At p=0
        // the spiral passes exactly through (s.posX, s.posY).
        captured_ = true;
        captureT_ = 0;
        float rdx = s.posX - cx_;
        float rdy = s.posY - cy_;
        captureRad_ = sqrtf(rdx * rdx + (rdy / SQUASH) * (rdy / SQUASH));
        startAng_ = atan2f(rdy / SQUASH, rdx);
        captureScale_ = s.scale;
        s.velX = 0;
        s.velY = 0;
    }

    // Captured: scripted vortex. The ship rides a logarithmic spiral from the
    // capture radius down to the core rim, turning like the drawn arms,
    // shrinking as it falls and keeping its nose pointed at the nucleus. No
    // thrust or stick can escape it; the hole swallows it at the rim.
    captureT_ += GAME_DT;
    float p = captureT_ / WORMHOLE_VORTEX_T;
    if (p > 1.0f) p = 1.0f;
    float rr = WORMHOLE_SWALLOW_R +
               (captureRad_ - WORMHOLE_SWALLOW_R) * (1.0f - p);
    float ang = startAng_ + (float)spin_ * p * WORMHOLE_VORTEX_TURNS * TAU;
    s.posX = cx_ + cosf(ang) * rr;
    s.posY = cy_ + sinf(ang) * rr * SQUASH;
    s.scale = captureScale_ * (1.0f - 0.75f * p); // shrink to 25%
    if (s.scale < 0.2f) s.scale = 0.2f;
    // Nose aims at the nucleus (thrust dir = (sin rad, -cos rad) with rad in
    // degrees): point (sin, -cos) toward the core, clamped to the ship's
    // rotation range.
    {
        float rot = atan2f(-cosf(ang), sinf(ang)) * 180.0f / PI;
        if (rot > 90.0f) rot = 90.0f;
        if (rot < -90.0f) rot = -90.0f;
        s.setTargetRotation(rot);
    }
    s.velX = 0;
    s.velY = 0;

    if (p >= 1.0f) {
        swallowed_ = true;
        phase_ = WH_SWALLOW;
        t_ = 0;
    }
    return true; // scripted: Game skips collisions during the vortex
}

void Wormhole::draw(Renderer &r, float viewX, float viewY, float viewScale) const
{
    if (!enabled_) return;

    // Phase fade: 0..1 brightness multiplier for the spiral/particles.
    float fade = 1.0f;
    float coreR = WORMHOLE_CORE_R;
    switch (phase_) {
    case WH_EMERGING: {
        float f = t_ / WORMHOLE_EMERGE_T;
        if (f > 1.0f) f = 1.0f;
        fade = f;
        coreR = WORMHOLE_CORE_R * (0.2f + 0.8f * f);
        break;
    }
    case WH_SWALLOW: {
        // Bright ring flash, then the disk dims quickly.
        float f = t_ / 0.15f;
        if (f > 1.0f) f = 1.0f;
        fade = 1.0f - 0.5f * f;
        break;
    }
    case WH_DYING: {
        float f = t_ / WORMHOLE_DIE_T;
        if (f > 1.0f) f = 1.0f;
        fade = 1.0f - f;
        coreR = WORMHOLE_CORE_R * (1.0f - 0.8f * f);
        break;
    }
    default:
        break;
    }
    if (fade <= 0.01f) return;

    float csx = cx_ * viewScale + viewX;
    float csy = cy_ * viewScale + viewY;

    // Deterministic pseudo-random jitter (per frame, stable drift).
    auto prand = [&](int k) -> float {
        float x = sinf((float)(k * 127 + 311) + phaseSeed_ + rot_ * 0.3f) * 43758.5453f;
        return x - (float)(int)x;
    };

    auto shadeSeg = [&](float x0, float y0, float x1, float y1, int b) {
        float dx = x1 - x0, dy = y1 - y0;
        float L = sqrtf(dx * dx + dy * dy);
        int n = (int)ceilf(L);
        if (n < 1) n = 1;
        for (int k = 0; k <= n; k++)
            r.pixelShade(x0 + dx * (float)k / (float)n,
                         y0 + dy * (float)k / (float)n, b);
    };

    const int M = 300;

    // Spiral arms: logarithmic spirals from the outer reach down to the core
    // rim. Dense and bright near the core (accretion glow), faint and sparse
    // at the outer edge. Squashed in Y so the disk reads as tilted.
    for (int arm = 0; arm < WORMHOLE_ARMS; arm++) {
        float px = 0.0f, py = 0.0f;
        bool open = false;
        for (int i = 0; i <= M; i++) {
            float tt = (float)i / (float)M; // 0 outer -> 1 core
            float ang = rot_ + (float)arm * TAU / (float)WORMHOLE_ARMS +
                        (float)spin_ * tt * WORMHOLE_TURNS * TAU;
            float rad = WORMHOLE_OUTER_R *
                        powf(WORMHOLE_CORE_R / WORMHOLE_OUTER_R, tt);
            float jit = (prand(i + arm * 131) - 0.5f) * 2.0f;
            float wx = cx_ + cosf(ang) * rad;
            float wy = cy_ + sinf(ang) * rad * SQUASH + jit * 0.8f;
            float sx = wx * viewScale + viewX;
            float sy = wy * viewScale + viewY;
            if (prand(i + arm * 17 + 300) > 0.93f) { // sparse gaps
                open = false;
                continue;
            }
            // Brighter toward the inner rim.
            float bb = powf(tt, 1.5f);
            int b = (int)(45.0f + 185.0f * bb * fade);
            if (b > 235) b = 235;
            if (b < 25) b = 25;
            if (open) shadeSeg(px, py, sx, sy, b);
            if (open) shadeSeg(px + 1.0f, py, sx + 1.0f, sy, b * 2 / 3);
            px = sx; py = sy; open = true;
        }
    }

    // Dark nucleus: a filled disc at brightness 0 swallows whatever is behind
    // it (stars, the inner spiral ends, terrain) — the event horizon. Drawn a
    // bit beyond the rim so the tight inner turns fall into the void.
    {
        const int NV = 28;
        float xs[NV], ys[NV];
        float rpx = coreR * 1.4f * viewScale;
        for (int k = 0; k < NV; k++) {
            float a = (float)k / (float)NV * TAU;
            xs[k] = csx + cosf(a) * rpx;
            ys[k] = csy + sinf(a) * rpx * SQUASH;
        }
        r.fillPolygon(xs, ys, NV, 0);
    }

    // Photon ring: a bright thin ELLIPSE at the core rim (same squash as the
    // spiral, so the whole disk reads tilted) with a dim outer halo, leaving
    // the interior dark — the black hole reads as a void framed by its glow.
    for (int k = 0; k <= 48; k++) {
        float a = (float)k / 48.0f * TAU;
        float ex = csx + cosf(a) * coreR * viewScale;
        float ey = csy + sinf(a) * coreR * viewScale * SQUASH;
        r.pixelShade(ex, ey, (int)(235 * fade));
    }
    r.pixelShade(csx, csy, (int)(90 * fade)); // faint backglow, not a bright core
    for (int k = 0; k < 16; k++) {
        float a = (float)k / 16.0f * TAU + rot_ * 0.25f;
        float rx = coreR * 1.32f * viewScale;
        int b = (int)(120.0f * fade * (0.6f + 0.4f * prand(k + 500)));
        r.pixelShade(csx + cosf(a) * rx, csy + sinf(a) * rx * SQUASH, b);
    }

    // Particles: bright specks riding the rotating arms, brightest and
    // closest-packed near the rim (falling into the hole).
    for (int p = 0; p < WORMHOLE_PARTICLES; p++) {
        int arm = p % WORMHOLE_ARMS;
        float tt = fmodf((float)p * (0.92f / (float)WORMHOLE_PARTICLES) +
                         rot_ * 0.0016f, 1.0f);
        if (tt < 0.0f) tt += 1.0f;
        float ang = rot_ + (float)arm * TAU / (float)WORMHOLE_ARMS +
                    (float)spin_ * tt * WORMHOLE_TURNS * TAU;
        float rad = WORMHOLE_OUTER_R *
                    powf(WORMHOLE_CORE_R / WORMHOLE_OUTER_R, tt);
        float wx = cx_ + cosf(ang) * rad;
        float wy = cy_ + sinf(ang) * rad * SQUASH;
        float sx = wx * viewScale + viewX;
        float sy = wy * viewScale + viewY;
        int b = (int)(120.0f + 130.0f * tt * fade);
        if (b > 250) b = 250;
        r.pixelShade(sx, sy, b);
        r.pixelShade(sx + 1.0f, sy, b * 2 / 3);
        r.pixelShade(sx - 1.0f, sy, b * 2 / 3);
    }
}
