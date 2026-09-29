#include <cmath>
#include <cstring>
#include "fogshow.h"
#include "config.h"
#include "terrain.h"
#include "renderer.h"

// TEMP showcase: four fog-band styles side by side on Titan, one per quadrant,
// so the look can be compared on the CRT and one picked. All four draw the
// SAME single band (FOG_BAND_START ellipse + drift) over the same Titan
// terrain; only the per-pixel style differs.

namespace {

static unsigned hash2(int x, int y)
{
    unsigned h = (unsigned)(x * 374761393u) ^ (unsigned)(y * 668265263u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return (h ^ (h >> 16)) & 0xff;
}

// 0..1 value noise (bilinear smoothstep over a 1x1 lattice).
static float vnoise(float x, float y)
{
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float fx = x - (float)xi, fy = y - (float)yi;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float a = hash2(xi, yi) / 255.0f;
    float b = hash2(xi + 1, yi) / 255.0f;
    float c = hash2(xi, yi + 1) / 255.0f;
    float d = hash2(xi + 1, yi + 1) / 255.0f;
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

// 2-octave fBm for the volumetric variant.
static float fbm(float x, float y)
{
    return 0.65f * vnoise(x, y) + 0.35f * vnoise(x * 2.3f + 7.1f, y * 2.3f + 3.3f);
}

// Gaussian density LUT (built once): u in [0, FOG_LUT_UMAX], exp -u^2*EXP.
static const unsigned char *gaussLUT()
{
    static unsigned char gauss[256];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; i++) {
            float u = (float)i / 255.0f * FOG_LUT_UMAX;
            gauss[i] = (unsigned char)(255.0f * expf(-u * u * FOG_GAUSS_EXP));
        }
        init = true;
    }
    return gauss;
}

// Single band center (world-y) at world-x, following the concentric ellipse
// plus a slow vertical drift (same as Atmosphere).
static float bandCenter(float wx, float t)
{
    float dx = wx - FOG_ELLIPSE_CX;
    float td = dx / FOG_ELLIPSE_RAD;
    if (td < -1.0f) td = -1.0f;
    if (td > 1.0f) td = 1.0f;
    float arc = FOG_CURVE_A * sqrtf(1.0f - td * td);
    return FOG_BAND_START - arc + FOG_DRIFT_A * sinf(t * 0.15f);
}

// Silhouette (screen-y) of the terrain under screen-x, from the view.
static float terrainScreenY(const Terrain &t, float wx, float viewY, float viewScale)
{
    float gy = t.yAt(wx, 1000.0f);
    return gy * viewScale + viewY;
}

// Shared per-column fog loop: for the given x (screen), returns the band's
// screen-y center, the per-pixel inverse unit, and the bottom clamp (silhouette).
struct BandCol {
    float cyScaled;
    float invunit;
    float bottom;
};

static bool bandColumn(const Terrain &t, float x, float viewX, float viewY, float viewScale,
                       float tsec, BandCol &out)
{
    float wx = ((float)x - viewX) / viewScale;
    float cyW = bandCenter(wx, tsec);
    out.cyScaled = cyW * viewScale + viewY;
    float drawHalf = FOG_DRAW_HALF * FOG_BAND_HALF * viewScale;
    out.invunit = 255.0f / (2.0f * drawHalf);
    out.bottom = terrainScreenY(t, wx, viewY, viewScale);
    return true;
}

static int bandDensity(const BandCol &c, int y, const unsigned char *gauss)
{
    float dyf = c.cyScaled - (float)y;
    if (dyf < 0.0f) dyf = -dyf;
    int idx = (int)(dyf * c.invunit);
    if (idx > 255) idx = 255;
    return gauss[idx];
}

// Variant 0 "DITHER": deterministic per-pixel stipple, density -> coverage.
// Fine stable dots; the classic halftone fog.
static void drawDither(Renderer &r, const Terrain &t, int x0, int x1, int y0, int y1,
                       float viewX, float viewY, float viewScale, float tsec)
{
    const unsigned char *gauss = gaussLUT();
    for (int x = x0; x < x1; x++) {
        BandCol c;
        if (!bandColumn(t, x, viewX, viewY, viewScale, tsec, c)) continue;
        for (int y = y0; y < y1; y++) {
            if (y >= (int)c.bottom - 1) break;
            int dens = bandDensity(c, y, gauss);
            if (dens == 0) continue;
            int thr = (dens * FOG_STIPPLE_MAX) >> 8;
            if (hash2(x, y) >= thr) continue;
            int bv = (int)(FOG_BRIGHT * (float)dens / 255.0f);
            if (bv < 4) bv = 4;
            r.pixelShade((float)x, (float)y, bv);
        }
    }
}

// Variant 1 "GRAIN": per-pixel noise drives BOTH coverage and brightness, so
// the band reads as coarse TV-static grain instead of uniform dots.
static void drawGrain(Renderer &r, const Terrain &t, int x0, int x1, int y0, int y1,
                      float viewX, float viewY, float viewScale, float tsec)
{
    const unsigned char *gauss = gaussLUT();
    for (int x = x0; x < x1; x++) {
        BandCol c;
        if (!bandColumn(t, x, viewX, viewY, viewScale, tsec, c)) continue;
        for (int y = y0; y < y1; y++) {
            if (y >= (int)c.bottom - 1) break;
            int dens = bandDensity(c, y, gauss);
            if (dens == 0) continue;
            if (hash2(x, y) >= dens) continue;
            unsigned b = hash2(x * 3 + 11, y * 5 + 7);
            int bv = (int)((float)b * FOG_BRIGHT / 255.0f);
            if (bv < 4) bv = 4;
            r.pixelShade((float)x, (float)y, bv);
        }
    }
}

// Variant 2 "RAGGED": low-frequency noise wiggles the band center per column
// (jagged organic edges) and modulates the density (irregular thickness).
static void drawRagged(Renderer &r, const Terrain &t, int x0, int x1, int y0, int y1,
                       float viewX, float viewY, float viewScale, float tsec)
{
    const unsigned char *gauss = gaussLUT();
    for (int x = x0; x < x1; x++) {
        float wx = ((float)x - viewX) / viewScale;
        float cyW = bandCenter(wx, tsec) + (vnoise(wx * 0.045f, tsec * 0.1f) - 0.5f) * 2.0f * FOG_BAND_HALF;
        float cyScaled = cyW * viewScale + viewY;
        float drawHalf = FOG_DRAW_HALF * FOG_BAND_HALF * viewScale;
        float invunit = 255.0f / (2.0f * drawHalf);
        float bottom = terrainScreenY(t, wx, viewY, viewScale);
        for (int y = y0; y < y1; y++) {
            if (y >= (int)bottom - 1) break;
            float dyf = cyScaled - (float)y;
            if (dyf < 0.0f) dyf = -dyf;
            int idx = (int)(dyf * invunit);
            if (idx > 255) idx = 255;
            float mod = 0.45f + 0.55f * vnoise(wx * 0.09f, (float)y * 0.02f);
            int dens = (int)((float)gauss[idx] * mod);
            if (dens == 0) continue;
            int thr = (dens * FOG_STIPPLE_MAX) >> 8;
            if (hash2(x, y) >= thr) continue;
            int bv = (int)(FOG_BRIGHT * (float)dens / 255.0f);
            if (bv < 4) bv = 4;
            r.pixelShade((float)x, (float)y, bv);
        }
    }
}

// Variant 3 "VOL 1-BIT": volumetric fBm density, drawn as pure 1-bit (fixed
// white luma when above threshold) — smoky, monochrome, lo-fi.
static void drawVol1bit(Renderer &r, const Terrain &t, int x0, int x1, int y0, int y1,
                        float viewX, float viewY, float viewScale, float tsec)
{
    const unsigned char *gauss = gaussLUT();
    const int ON_BRIGHT = 190;
    const int THRESHOLD = 90;
    for (int x = x0; x < x1; x++) {
        BandCol c;
        if (!bandColumn(t, x, viewX, viewY, viewScale, tsec, c)) continue;
        for (int y = y0; y < y1; y++) {
            if (y >= (int)c.bottom - 1) break;
            int base = bandDensity(c, y, gauss);
            if (base == 0) continue;
            float wx = ((float)x - viewX) / viewScale;
            float wy = ((float)y - viewY) / viewScale;
            float n = fbm(wx * 0.055f, wy * 0.055f + tsec * 0.03f);
            int dens = (int)((float)base * (0.35f + 1.3f * n));
            if (dens < THRESHOLD) continue;
            r.pixelShade((float)x, (float)y, ON_BRIGHT);
        }
    }
}

} // namespace

void FogShow::draw(Renderer &r, Terrain &t, float tsec)
{
    r.clear();
    const float viewScale = SCREEN_H / 700.0f;
    const float viewX = 0.0f;
    // Center the band vertically on screen: band center world-y at the
    // middle column is ~FOG_BAND_START.
    const float viewY = SCREEN_H * 0.5f - FOG_BAND_START * viewScale;

    // Shared background: stars + Titan terrain, once, full screen.
    t.drawStarField(r, viewX, viewY, viewScale);
    t.draw(r, viewX, viewY, viewScale, (int)(tsec * 60.0f), false, false, true);

    const int QW = SCREEN_W / 2, QH = SCREEN_H / 2;
    const char *labels[4] = { "1 DITHER", "2 GRAIN", "3 RAGGED", "4 VOL1BIT" };

    // Quadrant view: same world scene in all four (identical view), each with
    // its own style clipped to the quadrant.
    for (int q = 0; q < 4; q++) {
        int qx = (q % 2) * QW, qy = (q / 2) * QH;
        r.setClip((float)qx, (float)qy, (float)QW, (float)QH);
        switch (q) {
            case 0: drawDither(r, t, qx, qx + QW, qy, qy + QH, viewX, viewY, viewScale, tsec); break;
            case 1: drawGrain(r, t, qx, qx + QW, qy, qy + QH, viewX, viewY, viewScale, tsec); break;
            case 2: drawRagged(r, t, qx, qx + QW, qy, qy + QH, viewX, viewY, viewScale, tsec); break;
            case 3: drawVol1bit(r, t, qx, qx + QW, qy, qy + QH, viewX, viewY, viewScale, tsec); break;
        }
        r.clearClip();
        r.text((float)(qx + 4), (float)(qy + 4), labels[q]);
    }

    // Quadrant separators.
    r.rect(0.0f, (float)(QH - 1), (float)SCREEN_W, 2.0f);
    r.rect((float)(QW - 1), 0.0f, 2.0f, (float)SCREEN_H);
}