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
        bands_[i].cy = FOG_BAND_START + i * 120.0f;
        bands_[i].half = FOG_BAND_HALF_MIN;
        bands_[i].driftSpeed = FOG_DRIFT_SPEED_MIN;
        bands_[i].driftPhase = 0.0f;
        bands_[i].waveSpeed = FOG_WAVE_SPEED;
        bands_[i].wavePhase = 0.0f;
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
    // Spread the fog bands along the descent corridor (ship spawns at ~y=150
    // and descends to the pads), so every flight crosses at least one blind
    // zone. Each band gets its own drift/undulation so the pattern is not
    // memorizable.
    float y = FOG_BAND_START;
    for (int i = 0; i < FOG_BAND_COUNT; i++) {
        bands_[i].half = FOG_BAND_HALF_MIN +
                         randf01() * (FOG_BAND_HALF_MAX - FOG_BAND_HALF_MIN);
        bands_[i].cy = y + bands_[i].half;
        bands_[i].driftSpeed = FOG_DRIFT_SPEED_MIN +
                               randf01() * (FOG_DRIFT_SPEED_MAX - FOG_DRIFT_SPEED_MIN);
        bands_[i].driftPhase = randf01() * 6.2832f;
        bands_[i].waveSpeed = FOG_WAVE_SPEED * (0.5f + randf01());
        bands_[i].wavePhase = randf01() * 6.2832f;
        y = bands_[i].cy + bands_[i].half +
            FOG_BAND_GAP_MIN + randf01() * 50.0f;
    }
}

void Atmosphere::update(float dt)
{
    t_ += dt;
}

float Atmosphere::centerY(int i) const
{
    return bands_[i].cy +
           FOG_DRIFT_A * sinf(t_ * bands_[i].driftSpeed + bands_[i].driftPhase);
}

float Atmosphere::halfAt(int i, float x) const
{
    return bands_[i].half *
           (1.0f + FOG_WAVE_A * sinf(x * FOG_WAVE_K + t_ * bands_[i].waveSpeed + bands_[i].wavePhase));
}

bool Atmosphere::hidesShip(float x, float y) const
{
    if (!enabled_) return false;
    for (int i = 0; i < FOG_BAND_COUNT; i++) {
        float d = y - centerY(i);
        if (d < 0.0f) d = -d;
        if (d < halfAt(i, x)) return true;
    }
    return false;
}

void Atmosphere::drawSky(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const
{
    if (!enabled_) return;

    // Per-column terrain silhouette: the halo and the fog are only drawn in
    // the sky (above the silhouette) so they never darken the terrain itself.
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

    // Faint horizontal haze sheets whose thickness undulates along x and
    // whose whole band drifts vertically, matching hidesShip() exactly.
    for (int b = 0; b < FOG_BAND_COUNT; b++) {
        float cy = centerY(b);
        for (int x = 0; x < 320; x += 2) {
            float wx = ((float)x - viewX) / viewScale;
            float half = halfAt(b, wx);
            int yTop = (int)roundf((cy - half) * viewScale + viewY);
            int yBot = (int)roundf((cy + half) * viewScale + viewY);
            if (yBot < FOG_SCREEN_TOP || yTop > 240) continue;
            int y0 = yTop < FOG_SCREEN_TOP ? FOG_SCREEN_TOP : yTop;
            int y1 = yBot > 240 ? 240 : yBot;
            float span = (float)(yBot - yTop);
            for (int y = y0; y <= y1; y += 2) {
                if (y >= bottom[x / 2] - 1.0f) continue;
                float ty = ((float)y - (float)yTop) / (span + 1.0f);
                float prof = ty - 0.5f;
                if (prof < 0.0f) prof = -prof;
                float fade = 1.0f - prof * prof * 4.0f;
                if (fade < 0.0f) fade = 0.0f;
                int bv = (int)(8.0f + (FOG_BRIGHT - 8.0f) * fade);
                r.pixelShade((float)x, (float)y, bv);
            }
        }
    }
}
