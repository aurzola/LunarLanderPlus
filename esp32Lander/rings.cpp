#include <cmath>
#include <cstdlib>
#include "rings.h"
#include "moons.h"

Rings::Rings()
    : level_(1), enabled_(false), t_(0)
{
    rings_[0].radius = RING_RADIUS_INNER;
    rings_[0].w = RING_SPEED_INNER;
    rings_[0].phase = 0.0f;
    rings_[1].radius = RING_RADIUS_OUTER;
    rings_[1].w = RING_SPEED_OUTER;
    rings_[1].phase = 1.3f;
}

void Rings::reset(int level)
{
    level_ = level;
    enabled_ = moonHasRings(level);
    t_ = 0;
    // Random starting position so each level does not open with the same gap.
    rings_[0].phase = (float)(rand() % 1000) / 1000.0f * 6.2831853f;
    rings_[1].phase = (float)(rand() % 1000) / 1000.0f * 6.2831853f;
}

void Rings::update(float dt)
{
    if (!enabled_) return;
    t_ += dt;
    rings_[0].phase += rings_[0].w * dt;
    rings_[1].phase += rings_[1].w * dt;
}

int Rings::rocksInRing(int ringIndex) const
{
    return ringIndex == 0 ? RING_ROCKS_INNER : RING_ROCKS_OUTER;
}

void Rings::rockPos(int ringIndex, int rockIndex, float &x, float &y) const
{
    float a = rings_[ringIndex].phase + (float)rockIndex * 6.2831853f
              / (float)rocksInRing(ringIndex);
    x = RING_CX + rings_[ringIndex].radius * cosf(a);
    y = RING_CY + rings_[ringIndex].radius * sinf(a);
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

bool Rings::rockVisible(const Terrain &t, int ringIndex, int rockIndex, float &x, float &y) const
{
    rockPos(ringIndex, rockIndex, x, y);
    // Only the near side of the ring is above the surface; the far side is
    // hidden behind the moon, so it neither draws nor collides.
    return y < terrainYAt(t, x, 600.0f) - 1.0f;
}

void Rings::draw(Renderer &r, const Terrain &t, float viewX, float viewY, float viewScale) const
{
    if (!enabled_) return;
    float rpx = RING_ROCK_RADIUS * viewScale;
    if (rpx < (float)RING_ROCK_DRAW_MIN) rpx = (float)RING_ROCK_DRAW_MIN;
    int rad = (int)ceilf(rpx);

    for (int k = 0; k < RING_COUNT; k++) {
        int n = rocksInRing(k);
        for (int i = 0; i < n; i++) {
            float wx, wy;
            if (!rockVisible(t, k, i, wx, wy)) continue;
            float sx = wx * viewScale + viewX;
            float sy = wy * viewScale + viewY;
            if (sx < -30.0f || sx > SCREEN_W + 30.0f) continue;
            if (sy < -30.0f || sy > SCREEN_H + 30.0f) continue;

            // Filled rock: bright core with faint edge.
            for (int yy = -rad; yy <= rad; yy++) {
                for (int xx = -rad; xx <= rad; xx++) {
                    float d = sqrtf((float)(xx * xx + yy * yy));
                    if (d > rpx) continue;
                    int b = (d > rpx * 0.6f) ? 160 : 255;
                    r.pixelShade(sx + (float)xx, sy + (float)yy, b);
                }
            }
        }
    }
}

bool Rings::hitsShip(const Terrain &t, float sx, float sy, float shipR) const
{
    if (!enabled_) return false;
    float hit = RING_ROCK_RADIUS + shipR;
    float hit2 = hit * hit;
    for (int k = 0; k < RING_COUNT; k++) {
        int n = rocksInRing(k);
        for (int i = 0; i < n; i++) {
            float wx, wy;
            if (!rockVisible(t, k, i, wx, wy)) continue;
            float dx = wx - sx, dy = wy - sy;
            if (dx * dx + dy * dy < hit2) return true;
        }
    }
    return false;
}
