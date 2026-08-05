#include <cmath>
#include <cstdlib>
#include "storm.h"
#include "config.h"

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

Storm::Storm()
    : level_(1), nextBolt_(STORM_BOLT_MAX), spawned_(0)
{
}

float Storm::interval() const
{
    float base = STORM_BOLT_MAX - (float)(level_ - STORM_START_LEVEL) * 0.3f;
    return clampf(base, STORM_BOLT_MIN, STORM_BOLT_MAX);
}

void Storm::reset(int level)
{
    level_ = level;
    bolts_.clear();
    spawned_ = 0;
    nextBolt_ = interval();
}

float Storm::terrainYAt(const Terrain &t, float x, float fallback)
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

float Storm::segDist(float px, float py, float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1, dy = y2 - y1;
    float len2 = dx * dx + dy * dy;
    float t = (len2 > 0.0f)
        ? ((px - x1) * dx + (py - y1) * dy) / len2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float qx = x1 + t * dx, qy = y1 + t * dy;
    float ex = px - qx, ey = py - qy;
    return sqrtf(ex * ex + ey * ey);
}

bool Storm::strikes(float sx, float sy, float radius)
{
    for (int i = 0; i < (int)bolts_.size(); i++) {
        Bolt &b = bolts_[i];
        if (b.hit) continue;
        const std::vector<StormPoint> &p = b.path;
        if (p.size() < 2) continue;
        for (int k = 1; k < (int)p.size(); k++) {
            if (segDist(sx, sy, p[k - 1].x, p[k - 1].y, p[k].x, p[k].y) <= radius) {
                b.hit = true;
                return true;
            }
        }
    }
    return false;
}

void Storm::spawnBolt(const Terrain &t)
{
    float w = t.getWidth();
    float cloudX = randf01() * w;
    float startY = 30.0f + randf01() * 40.0f;
    float strikeX = cloudX + (randf01() - 0.5f) * 160.0f;
    strikeX = clampf(strikeX, 10.0f, w - 10.0f);
    float strikeY = terrainYAt(t, strikeX, 600.0f);
    if (strikeY < startY + 40.0f) strikeY = startY + 40.0f;

    Bolt b;
    b.maxLife = STORM_BOLT_LIFE * (0.7f + randf01() * 0.6f);
    b.life = b.maxLife;
    b.hit = false;

    int n = STORM_BOLT_SEGMENTS;
    b.path.reserve(n + 1);
    b.path.push_back(StormPoint());
    b.path[0].x = cloudX;
    b.path[0].y = startY;
    for (int k = 1; k <= n; k++) {
        float tt = (float)k / (float)n;
        StormPoint p;
        p.x = cloudX + (strikeX - cloudX) * tt
            + (randf01() - 0.5f) * STORM_BOLT_JITTER * (1.0f - tt * 0.6f + 0.1f);
        p.y = startY + (strikeY - startY) * tt
            + (randf01() - 0.5f) * 26.0f * (1.0f - tt + 0.25f);
        b.path.push_back(p);
    }
    b.path[n].x = strikeX;
    b.path[n].y = strikeY;

    bolts_.push_back(b);
    spawned_++;
}

void Storm::update(float dt, const Terrain &t)
{
    if (!active()) {
        bolts_.clear();
        return;
    }

    nextBolt_ -= dt;
    for (int i = 0; i < (int)bolts_.size();) {
        bolts_[i].life -= dt;
        if (bolts_[i].life <= 0.0f) {
            bolts_.erase(bolts_.begin() + i);
        } else {
            i++;
        }
    }

    if (nextBolt_ <= 0.0f) {
        spawnBolt(t);
        nextBolt_ = interval() * (0.7f + randf01() * 0.6f);
    }
}

void Storm::shadedLine(Renderer &r, float x0, float y0, float x1, float y1, int brightness)
{
    int x = (int)roundf(x0), y = (int)roundf(y0);
    int xe = (int)roundf(x1), ye = (int)roundf(y1);

    if (x == xe) {
        int step = y < ye ? 1 : -1;
        for (int cy = y; cy != ye + step; cy += step)
            r.pixelShade((float)x, (float)cy, brightness);
        return;
    }
    if (y == ye) {
        int step = x < xe ? 1 : -1;
        for (int cx = x; cx != xe + step; cx += step)
            r.pixelShade((float)cx, (float)y, brightness);
        return;
    }

    int dx = abs(xe - x), sx = x < xe ? 1 : -1;
    int dy = -abs(ye - y), sy = y < ye ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        r.pixelShade((float)x, (float)y, brightness);
        if (x == xe && y == ye) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

void Storm::drawPolyline(Renderer &r, const std::vector<StormPoint> &pts,
                         float viewX, float viewY, float viewScale, int brightness)
{
    if (pts.size() < 2) return;
    for (int i = 1; i < (int)pts.size(); i++) {
        shadedLine(r,
                   pts[i - 1].x * viewScale + viewX, pts[i - 1].y * viewScale + viewY,
                   pts[i].x * viewScale + viewX, pts[i].y * viewScale + viewY,
                   brightness);
    }
}

void Storm::drawBolt(Renderer &r, const Bolt &b, float viewX, float viewY,
                     float viewScale) const
{
    float lf = clampf(b.life / b.maxLife, 0.0f, 1.0f);
    float fade = (lf < STORM_BOLT_FADE) ? (lf / STORM_BOLT_FADE) : 1.0f;
    int glow = (int)(55.0f * fade);
    int core = (int)(185.0f * fade);

    drawPolyline(r, b.path, viewX + 1.0f, viewY, viewScale, glow);
    drawPolyline(r, b.path, viewX, viewY + 1.0f, viewScale, glow);
    drawPolyline(r, b.path, viewX, viewY, viewScale, core);
}

void Storm::drawBolts(Renderer &r, float viewX, float viewY, float viewScale) const
{
    if (!active()) return;
    for (int i = 0; i < (int)bolts_.size(); i++) {
        drawBolt(r, bolts_[i], viewX, viewY, viewScale);
    }
}

void Storm::drawSky(Renderer &r, float viewX, float viewY, float viewScale) const
{
    (void)r; (void)viewX; (void)viewY; (void)viewScale;
}
