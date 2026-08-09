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
    : level_(1), enabled_(false), t_(0.0f), width_(800.0f), frameCtr_(0)
{
    bands_[0].cy = RING_CY_HIGH;
    bands_[0].drift = RING_DRIFT_HIGH;
    bands_[0].smallCount = RING_SMALL_HIGH;
    bands_[0].dangerCount = RING_DANGER_HIGH;
    bands_[1].cy = RING_CY_LOW; // lower band: fixed concentric ellipse
    bands_[1].drift = RING_DRIFT_LOW;
    bands_[1].smallCount = RING_SMALL_LOW;
    bands_[1].dangerCount = RING_DANGER_LOW;
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

        // Small decorative rocks: fill the band, scattered, never collide.
        for (int i = 0; i < band.smallCount; i++) {
            Rock &rk = band.rocks[i];
            rk.x = (i + 0.5f) * (width_ / (float)band.smallCount) +
                   (randf01() * 2.0f - 1.0f) * 5.0f;
            if (rk.x < 0.0f) rk.x = 0.0f;
            if (rk.x >= width_) rk.x = width_ - 1.0f;
            rk.size = RING_SMALL_MIN_R + randf01() * (RING_SMALL_MAX_R - RING_SMALL_MIN_R);
            rk.yOff = (randf01() * 2.0f - 1.0f) * RING_Y_JITTER;
            rk.rot = randf01() * 6.2831853f;
            rk.spin = (randf01() * 2.0f - 1.0f) * RING_SPIN_MAX;
            rk.nVerts = 6;
            for (int v = 0; v < rk.nVerts; v++) rk.vrad[v] = 0.8f + randf01() * 0.3f;
        }

        // Big dangerous rocks: grouped close together but with a passable gap.
        int dn = band.dangerCount;
        float cell = width_ / (float)dn;
        for (int i = 0; i < dn; i++) {
            Rock &rk = band.rocks[band.smallCount + i];
            rk.x = (i + 0.5f) * cell + (randf01() * 2.0f - 1.0f) * cell * 0.16f;
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
            // Keep each danger rock clear of its neighbor so a gap remains.
            Rock &prev = band.rocks[band.smallCount + (i + dn - 1) % dn];
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
        int n = band.smallCount + band.dangerCount;
        for (int i = 0; i < n; i++) {
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
    return bands_[ringIndex].smallCount + bands_[ringIndex].dangerCount;
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

float Rings::bandY(const Terrain &t, int b, float x) const
{
    (void)t;
    // A ring around a moon is a smooth concentric ellipse: the arc bows upward
    // (peaks) over the moon's center and is symmetric, with no sharp edges.
    float dx = x - RING_ELLIPSE_CX;
    float tdx = dx / RING_ELLIPSE_RAD;
    if (tdx < -1.0f) tdx = -1.0f;
    if (tdx > 1.0f) tdx = 1.0f;
    float arc = RING_CURVE_A * sqrtf(1.0f - tdx * tdx);
    return bands_[b].cy - arc;
}

float Rings::rockY(const Terrain &t, int b, int i) const
{
    const Rock &rk = bands_[b].rocks[i];
    return bandY(t, b, rk.x) + rk.yOff;
}

bool Rings::rockVisible(const Terrain &t, int ringIndex, int rockIndex, float &x, float &y) const
{
    if (!enabled_) return false;
    if (ringIndex < 0 || ringIndex >= RING_COUNT) return false;
    if (rockIndex < 0 || rockIndex >= rocksInRing(ringIndex)) return false;
    x = bands_[ringIndex].rocks[rockIndex].x;
    y = rockY(t, ringIndex, rockIndex);
    return true;
}

bool Rings::rockDanger(int ringIndex, int rockIndex) const
{
    if (ringIndex < 0 || ringIndex >= RING_COUNT) return false;
    if (rockIndex < bands_[ringIndex].smallCount ||
        rockIndex >= rocksInRing(ringIndex)) return false;
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
    // rings). Soft gaussian falloff via a one-time 256-entry LUT so the inner
    // loop never calls expf() (software-emulated, very slow on the ESP32).
    static unsigned char gauss[256];
    static bool gaussInit = false;
    if (!gaussInit) {
        // u goes 0 (band center) .. 2 (band edge); fade = exp(-u^2 * 1.5).
        for (int i = 0; i < 256; i++) {
            float u = (float)i / 255.0f * 2.0f;
            gauss[i] = (unsigned char)(255.0f * expf(-u * u * 1.5f));
        }
        gaussInit = true;
    }

    float half = RING_FOG_HALF * viewScale;
    for (int b = 0; b < RING_COUNT; b++) {
        for (int x = 0; x < SCREEN_W; x++) {
            float wx = ((float)x - viewX) / viewScale;
            float cyScaled = bandY(t, b, wx) * viewScale + viewY;
            int gyScreen = (int)(terrainYAt(t, wx, (SCREEN_H - viewY) / viewScale + 100.0f) * viewScale + viewY);
            int y0 = (int)(cyScaled - half * 2.0f);
            int y1 = (int)(cyScaled + half * 2.0f);
            if (y1 < 0 || y0 > SCREEN_H) continue;
            if (y0 < 0) y0 = 0;
            if (y1 > SCREEN_H) y1 = SCREEN_H;
            // Map |y - cy| in the range [0, 2*half] onto the 0..255 LUT index.
            float invunit = 255.0f / (2.0f * half);
            for (int y = y0; y < y1; y++) {
                if (y >= gyScreen - 1) break; // stay above the terrain silhouette
                float dyf = cyScaled - (float)y;
                if (dyf < 0.0f) dyf = -dyf;
                int idx = (int)(dyf * invunit);
                if (idx > 255) idx = 255;
                unsigned char g = gauss[idx];
                if (g == 0) continue; // outside the soft fade (skip, don't stop)
                r.pixelShade((float)x, (float)y, (RING_FOG_BRIGHT * g) >> 8);
            }
        }
    }
}

void Rings::fillDanger(Renderer &r, const float *px, const float *py, int n) const
{
    // Soft-pattern fill for the big dangerous rocks: a gentle dither. First
    // the hollow outline, then a sparse interior dotting so it reads as a
    // solid-ish rock but stays subdued (not a sharp bright blob).
    tracePoly(r, px, py, n);
    float minx = px[0], maxx = px[0], miny = py[0], maxy = py[0];
    for (int i = 1; i < n; i++) {
        if (px[i] < minx) minx = px[i];
        if (px[i] > maxx) maxx = px[i];
        if (py[i] < miny) miny = py[i];
        if (py[i] > maxy) maxy = py[i];
    }
    int y0 = (int)ceilf(miny), y1 = (int)floorf(maxy);
    float xs[8];
    for (int yy = y0; yy <= y1; yy++) {
        float cy = yy + 0.5f;
        int m = 0;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            float y1p = py[i], y2p = py[j];
            if ((y1p <= cy && y2p > cy) || (y2p <= cy && y1p > cy)) {
                if (m < 8) xs[m++] = px[i] + (px[j] - px[i]) * (cy - y1p) / (y2p - y1p);
            }
        }
        if (m < 2) continue;
        for (int a = 0; a < m - 1; a++)
            for (int bb = a + 1; bb < m; bb++)
                if (xs[bb] < xs[a]) { float tmp = xs[a]; xs[a] = xs[bb]; xs[bb] = tmp; }
        for (int k = 0; k + 1 < m; k += 2) {
            int xa = (int)ceilf(xs[k]), xb = (int)floorf(xs[k + 1]);
            for (int xx = xa; xx <= xb; xx++)
                if (((xx + yy) & 2) == 0) r.pixelShade((float)xx, (float)yy, 170);
        }
    }
}

void Rings::draw(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const
{
    if (!enabled_) return;
    int frame = ++frameCtr_;

    drawFog(r, t, viewX, viewY, viewScale);

    for (int b = 0; b < RING_COUNT; b++) {
        if (b == 1 && (frame & 1)) continue;

        const Band &band = bands_[b];
        int n = band.smallCount + band.dangerCount;
        for (int i = 0; i < n; i++) {
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
            if (i >= band.smallCount) {
                // Big dangerous rock: soft-pattern fill.
                fillDanger(r, px, py, rk.nVerts);
            } else {
                // Small decorative rock: hollow only.
                tracePoly(r, px, py, rk.nVerts);
            }
        }
    }
}

bool Rings::hitsShip(const Terrain &t, float sx, float sy, float shipR) const
{
    if (!enabled_) return false;
    for (int b = 0; b < RING_COUNT; b++) {
        const Band &band = bands_[b];
        for (int i = band.smallCount; i < band.smallCount + band.dangerCount; i++) {
            const Rock &rk = band.rocks[i];
            if (rk.size <= 0.0f) continue;
            float dx = rk.x - sx;
            float dy = rockY(t, b, i) - sy;
            float hit = rk.size * RING_ROCK_HIT + shipR;
            if (dx * dx + dy * dy < hit * hit) return true;
        }
    }
    return false;
}
