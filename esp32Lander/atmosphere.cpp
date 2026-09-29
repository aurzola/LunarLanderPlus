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
    const float invVs2 = 1.0f / viewScale;
    for (int x = 0; x < 320; x += 2) {
        float wx = ((float)x - viewX) * invVs2;
        float gy = terrainYAt(t, wx, (240.0f - viewY) * invVs2 + 100.0f);
        int sy = (int)(gy * viewScale + viewY);
        if (sy > 240) sy = 240;
        bottom[x / 2] = (float)sy;

        if (sy < 240) {
            r.pixelShade((float)x, (float)(sy - 1), 20.0f);
            r.pixelShade((float)x, (float)(sy - 2), 9.0f);
        }
    }

    // One pass PER BAND (they never overlap, gap > 2·drawHalf), so the loop only
    // walks each band's own vertical extent instead of the union of all bands
    // (the old union loop paid 3 LUT lookups on every sky pixel, including the
    // empty gaps — the main FPS cost on the ESP32-S3).
    // Perf: the reciprocal of viewScale is hoisted out of the per-pixel loop
    // (a float division per pixel is expensive on the ESP32-S3).
    const float invViewScale = 1.0f / viewScale;
    // GRAIN anchored to SCREEN pixels: the hash uses (x, y) directly, so the
    // pattern is stable at any zoom and while the camera moves — a world hash
    // re-maps every pixel when the approach zoom kicks in and the particles
    // jumped/speeded up. The slow temporal drift slides the pattern down
    // gently (px/s), interpolated between adjacent cells for smooth motion.
    const float grainT = t_ * FOG_GRAIN_DRIFT;
    for (int b = 0; b < FOG_BAND_COUNT; b++) {
        for (int x = 0; x < SCREEN_W; x++) {
            float wx = ((float)x - viewX) * invViewScale;
            float wob = (noise1D(wx * 0.05f + (float)b * 3.7f) - 0.5f) * 2.0f * FOG_EDGE_AMP;
            float cyScaled = (centerAt(b, wx) + wob) * viewScale + viewY;
            float drawHalf = FOG_DRAW_HALF * bands_[b].half * viewScale;
            // The LUT reaches ~0 at u=FOG_LUT_UMAX; map that to the edge of the
            // drawn extent (drawHalf) so the density fades to zero exactly at
            // the border — otherwise the band was hard-clipped with density
            // still ~25% and showed a defined outline line in zoom.
            float invunit = 255.0f / drawHalf;
            int y0 = (int)(cyScaled - drawHalf);
            int y1 = (int)(cyScaled + drawHalf);
            if (y1 < 0 || y0 > SCREEN_H) continue;
            // The fog renders across the full screen height (no HUD clip): the
            // HUD text is drawn on top and clears its own label rectangles, so
            // the sky stays visible behind it instead of a black strip.
            if (y1 > SCREEN_H) y1 = SCREEN_H;
            int gx = x;
            int gx2 = x * 3 + 11;
            for (int y = y0; y < y1; y++) {
                if (y >= bottom[x / 2] - 1.0f) break; // above terrain silhouette
                float dyf = cyScaled - (float)y;
                if (dyf < 0.0f) dyf = -dyf;
                int idx = (int)(dyf * invunit);
                if (idx > 255) idx = 255;
                int dens = gauss[idx];
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
                // Screen-anchored grain with slow temporal drift: the pattern
                // slides down smoothly (interpolated between adjacent cells).
                float gyf = (float)y + grainT;
                int gy0 = (int)floorf(gyf);
                float frac = gyf - (float)gy0;
                unsigned h0 = grainHash(gx, gy0);
                unsigned h1 = grainHash(gx, gy0 + 1);
                unsigned h = (unsigned)((float)h0 + ((float)h1 - (float)h0) * frac);
                if (h >= (unsigned)dens) continue;
                float byf = (float)y * 5.0f + 7.0f + grainT * 5.0f;
                int by0 = (int)floorf(byf);
                float bfrac = byf - (float)by0;
                unsigned b0 = grainHash(gx2, by0);
                unsigned b1 = grainHash(gx2, by0 + 1);
                unsigned b = (unsigned)((float)b0 + ((float)b1 - (float)b0) * bfrac);
                int bv = (int)((float)b * FOG_BRIGHT / 255.0f);
                if (bv < 4) bv = 4;
                r.pixelShade((float)x, (float)y, bv);
            }
        }
    }
}
