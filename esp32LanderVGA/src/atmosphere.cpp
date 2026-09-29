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

// Deterministic per-pixel hash for the GRAIN fog: coverage (density threshold)
// and brightness both come from hashes of (x, y), so the band reads as coarse
// TV-static grain instead of a smooth gradient or uniform dots.
static unsigned grainHash(int x, int y)
{
    unsigned h = (unsigned)(x * 374761393u) ^ (unsigned)(y * 668265263u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return (h ^ (h >> 16)) & 0xff;
}

// Value-noise 1D (0..1, smoothstep-interpolated hashes) for the ragged band
// edges: low-frequency wobble that is world-anchored, so the same edge looks
// irregular at any zoom instead of going straight in the close-up view.
static float noise1D(float x)
{
    int xi = (int)floorf(x);
    float fx = x - (float)xi;
    fx = fx * fx * (3.0f - 2.0f * fx);
    unsigned a = grainHash(xi, 17);
    unsigned b = grainHash(xi + 1, 17);
    return ((float)a + ((float)b - (float)a) * fx) / 255.0f;
}

} // namespace

Atmosphere::Atmosphere()
    : level_(1), enabled_(false), t_(0.0f)
{
    for (int i = 0; i < FOG_BAND_COUNT; i++) {
        bands_[i].cy = FOG_BAND_START + i * (FOG_BAND_GAP_MIN + 80.0f);
        bands_[i].half = profileHalf(i);
        bands_[i].driftSpeed = FOG_DRIFT_SPEED_MIN;
        bands_[i].driftPhase = 0.0f;
    }
}

// Width profile: band 0 (closest to the terrain) is the narrowest; the top
// band is the widest, interpolated linearly in between.
float Atmosphere::profileHalf(int i)
{
    if (FOG_BAND_COUNT <= 1) return FOG_BAND_HALF;
    float t = (float)i / (float)(FOG_BAND_COUNT - 1);
    return FOG_BAND_HALF_BOTTOM + (FOG_BAND_HALF_TOP - FOG_BAND_HALF_BOTTOM) * t;
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
        bands_[i].half = profileHalf(i);
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

    // Gaussian density LUT (built once) replaces a per-pixel expf(). The LUT
    // value is used as a grain threshold (probability of drawing the dot at
    // this pixel), not a continuous brightness — the fog reads as coarse
    // TV-static grain (GRAIN style, picked from the fog showcase).
    static unsigned char gauss[256];
    static bool gaussInit = false;
    if (!gaussInit) {
        for (int i = 0; i < 256; i++) {
            float u = (float)i / 255.0f * FOG_LUT_UMAX;
            gauss[i] = (unsigned char)(255.0f * expf(-u * u * FOG_GAUSS_EXP));
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

        if (sy < 240) {
            r.pixelShade((float)x, (float)(sy - 1), 20.0f);
            r.pixelShade((float)x, (float)(sy - 2), 9.0f);
        }
    }

    // One pass per column over the union of all bands. Each band uses its own
    // half (width profile: narrower near the terrain), and the pixel density
    // is the max across bands so adjacent tails blend. Band centers wobble
    // with low-freq world noise so the edges are ragged at any zoom.
    float invunit[FOG_BAND_COUNT], cyScaled[FOG_BAND_COUNT];
    for (int x = 0; x < SCREEN_W; x++) {
        float wx = ((float)x - viewX) / viewScale;
        float yMin = 1e9f, yMax = -1e9f;
        for (int b = 0; b < FOG_BAND_COUNT; b++) {
            float wob = (noise1D(wx * 0.05f + (float)b * 3.7f) - 0.5f) * 2.0f * FOG_EDGE_AMP;
            cyScaled[b] = (centerAt(b, wx) + wob) * viewScale + viewY;
            float drawHalf = FOG_DRAW_HALF * bands_[b].half * viewScale;
            // The LUT reaches ~0 at u=FOG_LUT_UMAX; map that to the edge of the
            // drawn extent (drawHalf) so the density fades to zero exactly at
            // the border — otherwise the band was hard-clipped with density
            // still ~25% and showed a defined outline line in zoom.
            invunit[b] = 255.0f / drawHalf;
            if (cyScaled[b] - drawHalf < yMin) yMin = cyScaled[b] - drawHalf;
            if (cyScaled[b] + drawHalf > yMax) yMax = cyScaled[b] + drawHalf;
        }
        int y0 = (int)yMin, y1 = (int)yMax;
        if (y1 < 0 || y0 > SCREEN_H) continue;
        // The fog renders across the full screen height (no HUD clip): the HUD
        // text is drawn on top and clears its own label rectangles, so the sky
        // stays visible behind it instead of a black strip.
        if (y1 > SCREEN_H) y1 = SCREEN_H;
        for (int y = y0; y < y1; y++) {
            if (y >= bottom[x / 2] - 1.0f) break; // above terrain silhouette
            int dens = 0;
            for (int b = 0; b < FOG_BAND_COUNT; b++) {
                float dyf = cyScaled[b] - (float)y;
                if (dyf < 0.0f) dyf = -dyf;
                int idx = (int)(dyf * invunit[b]);
                if (idx > 255) idx = 255;
                int g = gauss[idx];
                if (g > dens) dens = g;
            }
            if (dens == 0) continue;
            // Ground fade: instead of a hard clip at the terrain silhouette,
            // scale the density to zero over FOG_GROUND_FADE_PX above it, so
            // the low band melts into the ground (no straight contour line).
            float gt = bottom[x / 2] - (float)y;
            if (gt < FOG_GROUND_FADE_PX) {
                int fade = (int)(255.0f * gt / FOG_GROUND_FADE_PX);
                if (fade < 4) fade = 4;
                dens = dens * fade / 255;
            }
            if (dens == 0) continue;
            // GRAIN anchored to WORLD space: hash world coordinates scaled by
            // FOG_GRAIN_W so the texture is identical at every zoom (the old
            // screen-pixel hash re-sampled on zoom and the edges went straight).
            float wy = ((float)y - viewY) / viewScale;
            unsigned h = grainHash((int)(wx * FOG_GRAIN_W), (int)(wy * FOG_GRAIN_W));
            if (h >= (unsigned)dens) continue;
            int bv = (int)((float)grainHash((int)(wx * FOG_GRAIN_W * 3.0f + 11.0f),
                                            (int)(wy * FOG_GRAIN_W * 5.0f + 7.0f)) *
                           FOG_BRIGHT / 255.0f);
            if (bv < 4) bv = 4;
            r.pixelShade((float)x, (float)y, bv);
        }
    }
}
