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
    : level_(1), enabled_(false), t_(0.0f), width_(800.0f)
{
    bands_[0].cy = RING_CY_HIGH;
    bands_[0].drift = RING_DRIFT_HIGH;
    bands_[0].count = RING_ROCKS_HIGH;
    bands_[1].cy = RING_CY_LOW;
    bands_[1].drift = RING_DRIFT_LOW;
    bands_[1].count = RING_ROCKS_LOW;
}

void Rings::reset(int level, const Terrain &t)
{
    level_ = level;
    enabled_ = moonHasRings(level);
    t_ = 0.0f;
    if (!enabled_) return;

    width_ = t.getWidth();
    if (width_ < 600.0f) width_ = 600.0f;
    float minGap = RING_GAP_MIN;

    for (int b = 0; b < RING_COUNT; b++) {
        Band &band = bands_[b];
        int n = band.count;
        float cell = width_ / (float)n;
        for (int i = 0; i < n; i++) {
            Rock &rk = band.rocks[i];
            rk.x = (i + 0.5f) * cell + (randf01() * 2.0f - 1.0f) * cell * 0.18f;
            if (rk.x < 0.0f) rk.x = 0.0f;
            if (rk.x >= width_) rk.x = width_ - 1.0f;
            rk.size = RING_DANGER_MIN_R + randf01() * (RING_DANGER_MAX_R - RING_DANGER_MIN_R);
            rk.yOff = (randf01() * 2.0f - 1.0f) * RING_Y_JITTER;
            rk.rot = randf01() * 6.2831853f;
            rk.spin = (randf01() * 2.0f - 1.0f) * RING_SPIN_MAX;
            rk.nVerts = 6;
            for (int v = 0; v < rk.nVerts; v++) {
                float baseRad = (v % 2 == 0) ? 1.05f : 0.75f;
                rk.vrad[v] = baseRad + (randf01() * 2.0f - 1.0f) * 0.25f;
                if (rk.vrad[v] < 0.6f) rk.vrad[v] = 0.6f;
            }
            // Keep each rock clear of its neighbor so a gap remains.
            Rock &prev = band.rocks[(i + n - 1) % n];
            float gapR = rk.x - prev.x;
            if (gapR < 0.0f) gapR += width_;
            if (gapR < minGap + rk.size + prev.size) {
                float shrink = (minGap + rk.size + prev.size - gapR) * 0.5f;
                if (shrink > 0.0f) rk.size -= shrink;
                if (rk.size < RING_DANGER_MIN_R) rk.size = RING_DANGER_MIN_R;
            }
        }
    }
}

void Rings::update(float dt)
{
    if (!enabled_) return;
    t_ += dt;
    for (int b = 0; b < RING_COUNT; b++) {
        Band &band = bands_[b];
        for (int i = 0; i < band.count; i++) {
            Rock &rk = band.rocks[i];
            rk.x += band.drift * dt;
            if (rk.x >= width_) rk.x -= width_;
            if (rk.x < 0.0f) rk.x += width_;
            rk.rot += rk.spin * dt;
        }
    }
}

int Rings::rocksInRing(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= RING_COUNT) return 0;
    return bands_[ringIndex].count;
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

float Rings::bandY(int b, float x) const
{
    // A ring around a moon is a smooth concentric ellipse: the arc bows upward
    // (peaks) over the moon's center and is symmetric, with no sharp edges.
    float dx = x - RING_ELLIPSE_CX;
    float t = dx / RING_ELLIPSE_RAD;
    if (t < -1.0f) t = -1.0f;
    if (t > 1.0f) t = 1.0f;
    float arc = RING_CURVE_A * sqrtf(1.0f - t * t);
    return bands_[b].cy - arc;
}

float Rings::rockY(const Terrain &t, int b, int i) const
{
    (void)t;
    const Rock &rk = bands_[b].rocks[i];
    return bandY(b, rk.x) + rk.yOff;
}

bool Rings::rockVisible(const Terrain &t, int ringIndex, int rockIndex, float &x, float &y) const
{
    if (!enabled_) return false;
    if (ringIndex < 0 || ringIndex >= RING_COUNT) return false;
    if (rockIndex < 0 || rockIndex >= bands_[ringIndex].count) return false;
    x = bands_[ringIndex].rocks[rockIndex].x;
    y = rockY(t, ringIndex, rockIndex);
    return true;
}

bool Rings::rockDanger(int ringIndex, int rockIndex) const
{
    if (ringIndex < 0 || ringIndex >= RING_COUNT) return false;
    if (rockIndex < 0 || rockIndex >= bands_[ringIndex].count) return false;
    return true;
}

void Rings::tracePoly(Renderer &r, const float *px, const float *py, int n) const
{
    // Hollow rock: draw only the outline (closed polygon), no fill.
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        r.line(px[i], py[i], px[j], py[j]);
    }
}

void Rings::drawFog(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const
{
    // Denser debris fog bands following a smooth concentric ellipse (like real
    // rings). The band uses a wide soft gradient so the edges fade out gently
    // instead of being cut off sharply.
    float half = RING_FOG_HALF * viewScale;
    for (int b = 0; b < RING_COUNT; b++) {
        for (int x = 0; x < SCREEN_W; x++) {
            float wx = ((float)x - viewX) / viewScale;
            float cyScaled = bandY(b, wx) * viewScale + viewY;
            int gyScreen = (int)(terrainYAt(t, wx, (SCREEN_H - viewY) / viewScale + 100.0f) * viewScale + viewY);
            int y0 = (int)(cyScaled - half * 2.0f);
            int y1 = (int)(cyScaled + half * 2.0f);
            if (y1 < 0 || y0 > SCREEN_H) continue;
            if (y0 < 0) y0 = 0;
            if (y1 > SCREEN_H) y1 = SCREEN_H;
            for (int y = y0; y < y1; y++) {
                if (y >= gyScreen - 1) break; // stay above the terrain silhouette
                float dy = ((float)y - cyScaled) / half;
                float fade = expf(-dy * dy * 1.5f); // gaussian -> soft edges
                if (fade < 0.03f) fade = 0.0f;
                int bv = (int)(RING_FOG_BRIGHT * fade);
                if (bv > 0) r.pixelShade((float)x, (float)y, bv);
            }
        }
    }
}

void Rings::draw(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const
{
    if (!enabled_) return;

    drawFog(r, t, viewX, viewY, viewScale);

    for (int b = 0; b < RING_COUNT; b++) {
        const Band &band = bands_[b];
        for (int i = 0; i < band.count; i++) {
            const Rock &rk = band.rocks[i];
            float sx = rk.x * viewScale + viewX;
            float sy = rockY(t, b, i) * viewScale + viewY;
            if (sx < -30.0f || sx > SCREEN_W + 30.0f) continue;
            if (sy < -30.0f || sy > SCREEN_H + 30.0f) continue;

            float px[MAX_VERTS], py[MAX_VERTS];
            for (int v = 0; v < rk.nVerts; v++) {
                float a = rk.rot + (float)v * 6.2831853f / (float)rk.nVerts;
                float rr = rk.size * rk.vrad[v] * viewScale;
                px[v] = sx + cosf(a) * rr;
                py[v] = sy + sinf(a) * rr;
            }
            tracePoly(r, px, py, rk.nVerts);
        }
    }
}

bool Rings::hitsShip(const Terrain &t, float sx, float sy, float shipR) const
{
    if (!enabled_) return false;
    for (int b = 0; b < RING_COUNT; b++) {
        const Band &band = bands_[b];
        for (int i = 0; i < band.count; i++) {
            const Rock &rk = band.rocks[i];
            if (rk.size <= 0.0f) continue;
            float dx = rk.x - sx;
            float dy = rockY(t, b, i) - sy;
            float hit = rk.size + shipR;
            if (dx * dx + dy * dy < hit * hit) return true;
        }
    }
    return false;
}
