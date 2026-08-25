#include <cmath>
#include <cstdlib>
#include "geysers.h"
#include "config.h"
#include "moons.h"

namespace {

float randf01()
{
    return (float)(rand() % 10000) / 10000.0f;
}

float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

} // namespace

Geysers::Geysers()
    : level_(1), enabled_(false), t_(0.0f)
{
}

float Geysers::terrainYAt(const Terrain &t, float x, float fallback)
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

void Geysers::reset(int level, const Terrain &t)
{
    level_ = level;
    enabled_ = moonHasGeysers(level);
    parts_.clear();
    vents_.clear();
    if (!enabled_) return;

    const std::vector<TerrainLine> &tl = t.getLines();
    float w = t.getWidth();

    std::vector<Vent> candidates;
    for (int i = 0; i < (int)tl.size(); i++) {
        const TerrainLine &l = tl[i];
        if (l.labelX < 0) continue;
        int j = i;
        float zoneEnd = l.x2;
        while (j + 1 < (int)tl.size() && tl[j + 1].landable) {
            j++;
            zoneEnd = tl[j].x2;
        }

        float lx = clampf(l.x1 - GEYSER_VENT_OFFSET, 4.0f, w - 4.0f);
        float rx = clampf(zoneEnd + GEYSER_VENT_OFFSET, 4.0f, w - 4.0f);

        Vent a;
        a.x = lx;
        a.gy = terrainYAt(t, lx, 500.0f);
        Vent b;
        b.x = rx;
        b.gy = terrainYAt(t, rx, 500.0f);
        candidates.push_back(a);
        candidates.push_back(b);
    }

    if (candidates.empty()) return;
    int n = (int)candidates.size() < GEYSER_VENTS ? (int)candidates.size() : GEYSER_VENTS;
    for (int i = 0; i < n; i++) {
        Vent v = candidates[i];
        v.erupting = (rand() % 2) == 0;
        v.age = 0.0f;
        if (v.erupting) v.timer = randf01() * GEYSER_BURST;
        else v.timer = randf01() * (GEYSER_GAP_MAX - GEYSER_GAP_MIN);
        vents_.push_back(v);
    }
}

bool Geysers::inPlume(float x, float y) const
{
    if (!enabled_) return false;
    for (int i = 0; i < (int)vents_.size(); i++) {
        const Vent &v = vents_[i];
        if (!v.erupting) continue;
        if (fabsf(x - v.x) > GEYSER_RADIUS) continue;
        if (y < v.gy - GEYSER_PLUME_H || y > v.gy) continue;
        return true;
    }
    return false;
}

void Geysers::emit(const Vent &v)
{
    Particle p;
    p.x = v.x + (randf01() - 0.5f) * 2.0f;
    p.y = v.gy - 1.0f;
    p.vx = (randf01() - 0.5f) * GEYSER_PART_SPREAD * 2.0f;
    p.vy = -GEYSER_PART_SPEED * (0.6f + randf01() * 0.8f);
    p.maxLife = GEYSER_PART_LIFE * (0.5f + randf01());
    p.life = p.maxLife;
    parts_.push_back(p);
}

void Geysers::update(float dt)
{
    t_ += dt;
    if (!enabled_) {
        parts_.clear();
        return;
    }

    for (int i = 0; i < (int)vents_.size(); i++) {
        Vent &v = vents_[i];
        v.timer -= dt;
        if (v.timer <= 0.0f) {
            if (v.erupting) {
                v.erupting = false;
                v.timer = GEYSER_GAP_MIN + randf01() * (GEYSER_GAP_MAX - GEYSER_GAP_MIN);
            } else {
                v.erupting = true;
                v.age = 0.0f;
                v.timer = GEYSER_BURST;
            }
        }
        if (v.erupting) {
            v.age += dt;
            if ((int)parts_.size() < GEYSER_MAX_PARTS && (rand() % 2) == 0) emit(v);
        }
    }

    for (int i = 0; i < (int)parts_.size();) {
        Particle &p = parts_[i];
        p.life -= dt;
        if (p.life <= 0.0f) {
            parts_.erase(parts_.begin() + i);
        } else {
            p.vy += GEYSER_PART_GRAV * dt;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            i++;
        }
    }
}

void Geysers::draw(Renderer &r, float viewX, float viewY, float viewScale) const
{
    if (!enabled_) return;

    for (int i = 0; i < (int)vents_.size(); i++) {
        const Vent &v = vents_[i];

        int sx = (int)roundf(v.x * viewScale + viewX);
        int sy = (int)roundf(v.gy * viewScale + viewY);

        float pulse = 0.85f + 0.15f * sinf(t_ * 2.0f + (float)i);
        int glow = (int)(190.0f * pulse + 40.0f);
        r.pixelShade((float)sx, (float)sy - 1.0f, glow);
        r.pixelShade((float)sx - 1.0f, (float)sy, glow / 2);
        r.pixelShade((float)sx + 1.0f, (float)sy, glow / 2);
        r.pixelShade((float)sx, (float)sy - 2.0f, glow / 2);

        if (!v.erupting) continue;

        float flash = (v.age < GEYSER_FLASH) ? (1.0f - v.age / GEYSER_FLASH) : 0.0f;
        if (flash > 0.0f) {
            int rad = 3;
            for (int dy = -rad; dy <= rad; dy++) {
                for (int dx = -rad; dx <= rad; dx++) {
                    if (dx * dx + dy * dy > rad * rad) continue;
                    int b = (int)(230.0f * flash * (1.0f - sqrtf((float)(dx * dx + dy * dy)) / (float)(rad + 1)));
                    r.pixelShade((float)(sx + dx), (float)(sy + dy), b);
                }
            }
        }
    }

    for (int i = 0; i < (int)parts_.size(); i++) {
        const Particle &p = parts_[i];
        float lf = clampf(p.life / p.maxLife, 0.0f, 1.0f);
        int b = (int)(40.0f + 180.0f * lf);
        r.pixelShade(p.x * viewScale + viewX, p.y * viewScale + viewY, b);
    }
}
