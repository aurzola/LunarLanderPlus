#include <cmath>
#include <cstdlib>
#include "atmosphere.h"
#include "config.h"
#include "moons.h"

namespace {

float randf01()
{
    return (float)(rand() % 10000) / 10000.0f;
}

} // namespace

Atmosphere::Atmosphere()
    : level_(1), enabled_(false), t_(0.0f)
{
    for (int i = 0; i < FOG_BAND_COUNT; i++) {
        bands_[i].cy = FOG_BAND_START + i * (FOG_BAND_GAP_MIN + 80.0f);
        bands_[i].half = FOG_BAND_HALF;
        bands_[i].driftSpeed = FOG_DRIFT_SPEED_MIN;
        bands_[i].driftPhase = 0.0f;
    }
}

float Atmosphere::terrainYAt(const Terrain &t, float x, float fallback)
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

void Atmosphere::reset(int level)
{
    level_ = level;
    enabled_ = moonHasTitan(level);
    t_ = 0.0f;
    float y = FOG_BAND_START;
    for (int i = 0; i < FOG_BAND_COUNT; i++) {
        bands_[i].half = FOG_BAND_HALF;
        bands_[i].cy = y;
        bands_[i].driftSpeed = FOG_DRIFT_SPEED_MIN +
                               randf01() * (FOG_DRIFT_SPEED_MAX - FOG_DRIFT_SPEED_MIN);
        bands_[i].driftPhase = randf01() * 6.2832f;
        y = bands_[i].cy + bands_[i].half + bands_[i].half +
            FOG_BAND_GAP_MIN + randf01() * 50.0f;
    }
}

void Atmosphere::update(float dt)
{
    t_ += dt;
}

float Atmosphere::centerAt(int i, float x) const
{
    // Concentric ellipse so the fog hugs the moon like a ring, plus a slow
    // vertical drift so the blind zones can't be memorized.
    float dx = x - FOG_ELLIPSE_CX;
    float td = dx / FOG_ELLIPSE_RAD;
    if (td < -1.0f) td = -1.0f;
    if (td > 1.0f) td = 1.0f;
    float arc = FOG_CURVE_A * sqrtf(1.0f - td * td);
    return bands_[i].cy - arc +
           FOG_DRIFT_A * sinf(t_ * bands_[i].driftSpeed + bands_[i].driftPhase);
}

bool Atmosphere::hidesShip(float x, float y) const
{
    if (!enabled_) return false;
    for (int i = 0; i < FOG_BAND_COUNT; i++) {
        float d = y - centerAt(i, x);
        if (d < 0.0f) d = -d;
        if (d < bands_[i].half) return true;
    }
    return false;
}

void Atmosphere::drawSky(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const
{
    if (!enabled_) return;

    // Gaussian LUT (built once) replaces a per-pixel expf() for the falloff.
    static unsigned char gauss[256];
    static bool gaussInit = false;
    if (!gaussInit) {
        for (int i = 0; i < 256; i++) {
            float u = (float)i / 255.0f * 2.0f;
            gauss[i] = (unsigned char)(255.0f * expf(-u * u * 1.5f));
        }
        gaussInit = true;
    }

    // Per-column terrain silhouette: fog is only drawn above the terrain.
    float bottom[160];
    for (int x = 0; x < 320; x += 2) {
        float wx = ((float)x - viewX) / viewScale;
        float gy = terrainYAt(t, wx, (240.0f - viewY) / viewScale + 100.0f);
        int sy = (int)(gy * viewScale + viewY);
        if (sy > 240) sy = 240;
        bottom[x / 2] = (float)sy;

        if (sy >= FOG_SCREEN_TOP + 2 && sy < 240) {
            r.pixelShade((float)x, (float)(sy - 1), 20.0f);
            r.pixelShade((float)x, (float)(sy - 2), 9.0f);
        }
    }

    float half = FOG_BAND_HALF * viewScale;
    for (int b = 0; b < FOG_BAND_COUNT; b++) {
        for (int x = 0; x < SCREEN_W; x++) {
            float wx = ((float)x - viewX) / viewScale;
            float cyScaled = centerAt(b, wx) * viewScale + viewY;
            int y0 = (int)(cyScaled - half * 2.0f);
            int y1 = (int)(cyScaled + half * 2.0f);
            if (y1 < 0 || y0 > SCREEN_H) continue;
            if (y0 < FOG_SCREEN_TOP) y0 = FOG_SCREEN_TOP; // clear of the HUD
            if (y1 > SCREEN_H) y1 = SCREEN_H;
            float invunit = 255.0f / (2.0f * half);
            for (int y = y0; y < y1; y++) {
                if (y >= bottom[x / 2] - 1.0f) break; // above terrain silhouette
                float dyf = cyScaled - (float)y;
                if (dyf < 0.0f) dyf = -dyf;
                int idx = (int)(dyf * invunit);
                if (idx > 255) idx = 255;
                unsigned char g = gauss[idx];
                if (g == 0) continue;
                int bv = (int)(FOG_BRIGHT * (float)g) >> 8;
                r.pixelShade((float)x, (float)y, bv);
            }
        }
    }
}
