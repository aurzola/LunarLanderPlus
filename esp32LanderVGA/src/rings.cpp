#include <cmath>
#include <cstdlib>
#include "rings.h"
#include "moons.h"

namespace {

float randf01()
{
    return (float)(rand() % 10000) / 10000.0f;
}

} // namespace

Rings::Rings()
    : level_(1), enabled_(false), t_(0.0f), width_(800.0f),
      cy_(RING_CY), drift_(RING_DRIFT),
      smallCount_(RING_SMALL_COUNT), dangerCount_(RING_DANGER_COUNT)
{
}

void Rings::reset(int level, const Terrain &t)
{
    level_ = level;
    enabled_ = moonHasRings(level);
    t_ = 0.0f;
    if (!enabled_) return;

    width_ = t.getWidth();
    if (width_ < 600.0f) width_ = 600.0f;
    cy_ = RING_CY;
    drift_ = RING_DRIFT;
    smallCount_ = RING_SMALL_COUNT;
    dangerCount_ = RING_DANGER_COUNT;

    int total = smallCount_ + dangerCount_;
    if (total > MAX_ROCKS) {
        smallCount_ = MAX_ROCKS / 2;
        dangerCount_ = MAX_ROCKS - smallCount_;
        total = MAX_ROCKS;
    }

    // Small decorative rocks: upper half of the band [-JITTER, 0].
    // Fully random X positions across the world width, no grid.
    for (int i = 0; i < smallCount_; i++) {
        Rock &rk = rocks_[i];
        rk.x = randf01() * width_;
        rk.size = RING_SMALL_MIN_R + randf01() * (RING_SMALL_MAX_R - RING_SMALL_MIN_R);
        rk.yOff = -(randf01() * RING_Y_JITTER);  // upper half: negative offset
        rk.rot = randf01() * 6.2831853f;
        rk.spin = (randf01() * 2.0f - 1.0f) * RING_SPIN_MAX * 1.5f;
        rk.nVerts = 4 + (rand() % 5);  // 4–8 vertices
        for (int v = 0; v < rk.nVerts; v++) rk.vrad[v] = 0.5f + randf01() * 0.8f;
    }

    // Big dangerous rocks: lower half [0, +JITTER], grid-based with heavy
    // jitter so gaps are guaranteed but rocks still look chaotic.
    float cell = width_ / (float)dangerCount_;
    for (int i = 0; i < dangerCount_; i++) {
        Rock &rk = rocks_[smallCount_ + i];
        rk.x = (i + 0.5f) * cell + (randf01() * 2.0f - 1.0f) * cell * 0.38f;
        if (rk.x < 0.0f) rk.x += width_;
        if (rk.x >= width_) rk.x -= width_;
        rk.size = RING_DANGER_MIN_R + randf01() * (RING_DANGER_MAX_R - RING_DANGER_MIN_R);
        rk.yOff = randf01() * RING_Y_JITTER;  // lower half: positive offset
        rk.rot = randf01() * 6.2831853f;
        rk.spin = (randf01() * 2.0f - 1.0f) * RING_SPIN_MAX * 1.5f;
        rk.nVerts = 4 + (rand() % 5);  // 4–8 vertices
        for (int v = 0; v < rk.nVerts; v++) {
            float baseRad = (v % 2 == 0) ? 1.10f : 0.70f;
            rk.vrad[v] = baseRad + (randf01() * 2.0f - 1.0f) * 0.30f;
            if (rk.vrad[v] < 0.55f) rk.vrad[v] = 0.55f;
        }
    }
}

void Rings::update(float dt)
{
    if (!enabled_) return;
    t_ += dt;
    int total = smallCount_ + dangerCount_;
    for (int i = 0; i < total; i++) {
        Rock &rk = rocks_[i];
        rk.x += drift_ * dt;
        if (rk.x >= width_) rk.x -= width_;
        if (rk.x < 0.0f) rk.x += width_;
        rk.rot += rk.spin * dt;
    }
}

float Rings::terrainYAt(const Terrain &t, float x, float fallback)
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

float Rings::bandY(const Terrain &t, float x) const
{
    (void)t;
    float dx = x - RING_ELLIPSE_CX;
    float tdx = dx / RING_ELLIPSE_RAD;
    if (tdx < -1.0f) tdx = -1.0f;
    if (tdx > 1.0f) tdx = 1.0f;
    float arc = RING_CURVE_A * sqrtf(1.0f - tdx * tdx);
    return cy_ - arc;
}

float Rings::rockY(const Terrain &t, int i) const
{
    const Rock &rk = rocks_[i];
    return bandY(t, rk.x) + rk.yOff;
}

bool Rings::rockVisible(const Terrain &t, int rockIndex, float &x, float &y) const
{
    if (!enabled_) return false;
    int total = smallCount_ + dangerCount_;
    if (rockIndex < 0 || rockIndex >= total) return false;
    x = rocks_[rockIndex].x;
    y = rockY(t, rockIndex);
    return true;
}

bool Rings::rockDanger(int rockIndex) const
{
    int total = smallCount_ + dangerCount_;
    if (rockIndex < smallCount_ || rockIndex >= total) return false;
    return true;
}

void Rings::tracePoly(Renderer &r, const float *px, const float *py, int n) const
{
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        r.line(px[i], py[i], px[j], py[j]);
    }
}

void Rings::drawFog(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const
{
    static unsigned char gauss[256];
    static bool gaussInit = false;
    if (!gaussInit) {
        for (int i = 0; i < 256; i++) {
            float u = (float)i / 255.0f * 2.0f;
            gauss[i] = (unsigned char)(255.0f * expf(-u * u * 1.5f));
        }
        gaussInit = true;
    }

    float half = RING_FOG_HALF * viewScale;
    for (int x = 0; x < SCREEN_W; x++) {
        float wx = ((float)x - viewX) / viewScale;
        float cyScaled = bandY(t, wx) * viewScale + viewY;
        int gyScreen = (int)(terrainYAt(t, wx, (SCREEN_H - viewY) / viewScale + 100.0f) * viewScale + viewY);
        int y0 = (int)(cyScaled - half * 2.0f);
        int y1 = (int)(cyScaled + half * 2.0f);
        if (y1 < 0 || y0 > SCREEN_H) continue;
        if (y0 < 0) y0 = 0;
        if (y1 > SCREEN_H) y1 = SCREEN_H;
        float invunit = 255.0f / (2.0f * half);
        for (int y = y0; y < y1; y++) {
            if (y >= gyScreen - 1) break;
            float dyf = cyScaled - (float)y;
            if (dyf < 0.0f) dyf = -dyf;
            int idx = (int)(dyf * invunit);
            if (idx > 255) idx = 255;
            unsigned char g = gauss[idx];
            if (g == 0) continue;
            r.pixelShade((float)x, (float)y, (RING_FOG_BRIGHT * g) >> 8);
        }
    }
}

void Rings::fillDanger(Renderer &r, const float *px, const float *py, int n) const
{
    r.fillPolygon(px, py, n, 170);
    tracePoly(r, px, py, n);
}

void Rings::draw(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const
{
    if (!enabled_) return;

    drawFog(r, t, viewX, viewY, viewScale);

    int total = smallCount_ + dangerCount_;
    for (int i = 0; i < total; i++) {
        const Rock &rk = rocks_[i];
        float sx = rk.x * viewScale + viewX;
        float sy = rockY(t, i) * viewScale + viewY;
        if (sx < -30.0f || sx > SCREEN_W + 30.0f) continue;
        if (sy < -30.0f || sy > SCREEN_H + 30.0f) continue;

        float px[MAX_VERTS], py[MAX_VERTS];
        for (int v = 0; v < rk.nVerts; v++) {
            float a = rk.rot + (float)v * 6.2831853f / (float)rk.nVerts;
            float rr = rk.size * rk.vrad[v] * viewScale;
            px[v] = sx + cosf(a) * rr;
            py[v] = sy + sinf(a) * rr;
        }
        if (i >= smallCount_) {
            fillDanger(r, px, py, rk.nVerts);
        } else {
            tracePoly(r, px, py, rk.nVerts);
        }
    }
}

bool Rings::hitsShip(const Terrain &t, float sx, float sy, float shipR) const
{
    if (!enabled_) return false;
    for (int i = smallCount_; i < smallCount_ + dangerCount_; i++) {
        const Rock &rk = rocks_[i];
        if (rk.size <= 0.0f) continue;
        float dx = rk.x - sx;
        float dy = rockY(t, i) - sy;
        float hit = rk.size * RING_ROCK_HIT + shipR;
        if (dx * dx + dy * dy < hit * hit) return true;
    }
    return false;
}
