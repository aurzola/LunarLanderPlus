#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "volcanoes.h"
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

void drawFlowArm(Renderer &r, const std::vector<float> &fx, const std::vector<float> &fy,
                 float pulse, float viewX, float viewY, float viewScale)
{
    int m = (int)fx.size();
    for (int k = 1; k < m; k++) {
        float fade = 1.0f - 0.35f * (float)k / (float)m;
        float bx = fx[k - 1], by = fy[k - 1];
        float ex = fx[k], ey = fy[k];
        int steps = (int)ceilf(fabsf(ex - bx) / 2.0f);
        if (steps < 1) steps = 1;
        for (int s = 0; s <= steps; s++) {
            float tt = (float)s / (float)steps;
            float wx = bx + (ex - bx) * tt;
            float wy = by + (ey - by) * tt;
            int px = (int)roundf(wx * viewScale + viewX);
            int py = (int)roundf(wy * viewScale + viewY);
            float p = pulse * fade;
            int b1 = (int)(235.0f * p);
            int b2 = (int)(120.0f * p);
            int b3 = (int)(55.0f * p);
            r.pixelShade((float)px, (float)(py - 2), b1);
            r.pixelShade((float)px, (float)(py - 3), b1);
            r.pixelShade((float)px, (float)(py - 1), b2);
            r.pixelShade((float)px, (float)(py), b3);
        }
    }
}

} // namespace

Volcanoes::Volcanoes()
    : level_(1), enabled_(false), t_(0.0f)
{
}

float Volcanoes::terrainYAt(const Terrain &t, float x, float fallback)
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

void Volcanoes::buildFlow(const Terrain &t, Volcano &v, float w)
{
    v.flowX.clear();
    v.flowY.clear();
    v.flowX2.clear();
    v.flowY2.clear();

    // Lava always pours downhill from the peak: build one arm per side. The
    // arms are later clamped to stop at the lava boundary on a landing pad.
    for (int side = 0; side < 2; side++) {
        float dir = (side == 0) ? 1.0f : -1.0f;
        std::vector<float> &fx = (side == 0) ? v.flowX : v.flowX2;
        std::vector<float> &fy = (side == 0) ? v.flowY : v.flowY2;

        float x = v.x;
        float prevY = v.gy;
        fx.push_back(v.x);
        fy.push_back(v.gy);
        for (float dist = VOLCANO_FLOW_STEP; dist <= VOLCANO_FLOW_LEN; dist += VOLCANO_FLOW_STEP) {
            x += dir * VOLCANO_FLOW_STEP;
            if (x < 0.0f || x > w) break;
            float y = terrainYAt(t, x, prevY);
            if (y < prevY - 1.0f) break;   // terrain climbs again: stop this arm
            fx.push_back(x);
            fy.push_back(y);
            prevY = y;
        }
    }
}

void Volcanoes::computeLava(const Terrain &t)
{
    lava_.clear();
    if (!enabled_) return;

    const std::vector<TerrainLine> &tl = t.getLines();
    int n = (int)tl.size();

    // Landing pads: contiguous runs of landable lines (multiplier>1).
    std::vector<LavaRange> pads;
    for (int i = 0; i < n; i++) {
        if (!tl[i].landable || tl[i].multiplier <= 1) continue;
        float x1 = tl[i].x1, x2 = tl[i].x2;
        int j = i;
        while (j + 1 < n && tl[j + 1].landable && tl[j + 1].multiplier > 1) {
            j++;
            x2 = tl[j].x2;
        }
        pads.push_back({x1, x2});
        i = j;
    }

    // For each pad, determine which sides lava reaches it from (any volcano arm
    // entering the pad from the left / right). The burn zone on each side is
    // capped so a safe strip (VOLCANO_SAFE_STRIP) always stays clear, and it is
    // trimmed to the furthest point the flow actually reaches on the pad, so it
    // matches the visible ribbon exactly.
    for (int p = 0; p < (int)pads.size(); p++) {
        float padW = pads[p].x2 - pads[p].x1;
        bool leftR = false, rightR = false;
        float leftReach = pads[p].x1, rightReach = pads[p].x2;
        for (int v = 0; v < (int)volc_.size(); v++) {
            bool fromLeft = (volc_[v].x < pads[p].x1);
            const std::vector<float> *arms[2] = { &volc_[v].flowX, &volc_[v].flowX2 };
            for (int a = 0; a < 2; a++) {
                for (int k = 1; k < (int)arms[a]->size(); k++) {
                    float px = (*arms[a])[k];
                    if (px >= pads[p].x1 && px <= pads[p].x2) {
                        if (fromLeft) {
                            leftR = true;
                            if (px > leftReach) leftReach = px;
                        } else {
                            rightR = true;
                            if (px < rightReach) rightReach = px;
                        }
                    }
                }
            }
        }
        if (!leftR && !rightR) continue;

        float coverL = 0.0f, coverR = 0.0f;
        if (leftR && rightR) {
            coverL = (padW - VOLCANO_SAFE_STRIP) * 0.5f;
            coverR = (padW - VOLCANO_SAFE_STRIP) * 0.5f;
        } else if (leftR) {
            coverL = padW - VOLCANO_SAFE_STRIP;
        } else {
            coverR = padW - VOLCANO_SAFE_STRIP;
        }
        float lEdge = pads[p].x1 + coverL;
        if (lEdge > leftReach) lEdge = leftReach;
        if (lEdge > pads[p].x1) lava_.push_back({pads[p].x1, lEdge});
        float rEdge = pads[p].x2 - coverR;
        if (rEdge < rightReach) rEdge = rightReach;
        if (rEdge < pads[p].x2) lava_.push_back({rEdge, pads[p].x2});
    }

    // Merge overlapping ranges.
    std::sort(lava_.begin(), lava_.end(),
              [](const LavaRange &a, const LavaRange &b) { return a.x1 < b.x1; });
    std::vector<LavaRange> merged;
    for (int i = 0; i < (int)lava_.size(); i++) {
        if (merged.empty() || lava_[i].x1 > merged.back().x2) {
            merged.push_back(lava_[i]);
        } else if (lava_[i].x2 > merged.back().x2) {
            merged.back().x2 = lava_[i].x2;
        }
    }
    lava_ = merged;
}

void Volcanoes::clampFlows()
{
    // Truncate each lava arm so it stops at the far edge of the burn zone on
    // the pad it reaches. This keeps the visible ribbon consistent with the
    // safe strip: lava never flows across the clear part of a pad.
    for (int v = 0; v < (int)volc_.size(); v++) {
        Volcano &vc = volc_[v];
        for (int side = 0; side < 2; side++) {
            std::vector<float> &fx = (side == 0) ? vc.flowX : vc.flowX2;
            std::vector<float> &fy = (side == 0) ? vc.flowY : vc.flowY2;
            if (fx.size() < 2) continue;

            float boundary = 0.0f;
            bool haveBoundary = false;
            if (side == 0) {
                for (int i = 0; i < (int)lava_.size(); i++) {
                    if (lava_[i].x1 > vc.x) { boundary = lava_[i].x2; haveBoundary = true; break; }
                }
            } else {
                for (int i = (int)lava_.size() - 1; i >= 0; i--) {
                    if (lava_[i].x2 < vc.x) { boundary = lava_[i].x1; haveBoundary = true; break; }
                }
            }
            if (!haveBoundary) continue;

            std::vector<float> nfx, nfy;
            for (int k = 0; k < (int)fx.size(); k++) {
                if (side == 0) { if (fx[k] > boundary) break; }
                else { if (fx[k] < boundary) break; }
                nfx.push_back(fx[k]);
                nfy.push_back(fy[k]);
            }
            if (nfx.size() >= 2) {
                fx = nfx;
                fy = nfy;
            }
        }
    }
}

bool Volcanoes::landOnLava(float left, float right) const
{
    if (!enabled_) return false;
    for (int i = 0; i < (int)lava_.size(); i++) {
        if (right > lava_[i].x1 && left < lava_[i].x2) return true;
    }
    return false;
}

void Volcanoes::reset(int level, const Terrain &t)
{
    level_ = level;
    enabled_ = moonHasVolcanoes(level);
    t_ = 0.0f;
    parts_.clear();
    volc_.clear();
    if (!enabled_) return;

    const std::vector<TerrainLine> &tl = t.getLines();
    float w = t.getWidth();

    std::vector<float> candX;
    for (float x = 12.0f; x < w - 12.0f; x += 6.0f) {
        bool landable = false;
        for (int i = 0; i < (int)tl.size(); i++) {
            const TerrainLine &l = tl[i];
            if (!l.landable) continue;
            if (x >= l.x1 && x <= l.x2) { landable = true; break; }
        }
        if (landable) continue;
        float yc = terrainYAt(t, x, 9999.0f);
        float yl = terrainYAt(t, x - VOLCANO_PEAK_R, 9999.0f);
        float yr = terrainYAt(t, x + VOLCANO_PEAK_R, 9999.0f);
        if (yc >= 9990.0f || yl >= 9990.0f || yr >= 9990.0f) continue;
        // mountain peak: both sides lower (larger y) than x
        if (yl > yc + 2.0f && yr > yc + 2.0f) candX.push_back(x);
    }
    if (candX.empty()) return;

    int n = (int)candX.size() < VOLCANO_VENTS ? (int)candX.size() : VOLCANO_VENTS;
    for (int k = 0; k < n; k++) {
        float x = candX.size() == 1
            ? candX[0]
            : candX[(int)roundf((float)k * (float)((int)candX.size() - 1) / (float)(n - 1))];
        Volcano v;
        v.x = x;
        v.gy = terrainYAt(t, x, 500.0f);
        v.erupting = (rand() % 3) == 0;
        v.age = 0.0f;
        v.phase = randf01() * 6.2832f;
        if (v.erupting) v.timer = randf01() * VOLCANO_BURST;
        else v.timer = VOLCANO_GAP_MIN + randf01() * (VOLCANO_GAP_MAX - VOLCANO_GAP_MIN);
        buildFlow(t, v, w);
        if (v.flowX.size() < 2 && v.flowX2.size() < 2) continue;
        volc_.push_back(v);
    }
    computeLava(t);
    clampFlows();
}

void Volcanoes::rebuild(const Terrain &t)
{
    if (!enabled_) return;
    float w = t.getWidth();
    for (int i = 0; i < (int)volc_.size(); i++) {
        Volcano &v = volc_[i];
        // Re-anchor the crater on the (possibly re-shaped) surface and re-run
        // the downhill arms so the lava follows the new terrain. A pad that a
        // quake destroyed is no longer landable, so computeLava() drops its
        // range and clampFlows() stops holding the ribbon back there.
        v.gy = terrainYAt(t, v.x, v.gy);
        buildFlow(t, v, w);
    }
    computeLava(t);
    clampFlows();
}

void Volcanoes::emit(const Volcano &v)
{
    Particle p;
    // eruption rises from the center of the crater mouth, over a wider area
    p.x = v.x + (randf01() - 0.5f) * 7.0f;
    p.y = v.gy - 2.0f;
    p.vx = (randf01() - 0.5f) * 5.0f;
    p.vy = -VOLCANO_ERUPT_SPEED * (0.55f + randf01() * 0.9f);
    p.maxLife = VOLCANO_PART_LIFE * (0.6f + randf01() * 0.8f);
    p.life = p.maxLife;
    parts_.push_back(p);
}

void Volcanoes::update(float dt)
{
    t_ += dt;
    if (!enabled_) {
        parts_.clear();
        return;
    }

    for (int i = 0; i < (int)volc_.size(); i++) {
        Volcano &v = volc_[i];
        v.timer -= dt;
        if (v.timer <= 0.0f) {
            if (v.erupting) {
                v.erupting = false;
                v.timer = VOLCANO_GAP_MIN + randf01() * (VOLCANO_GAP_MAX - VOLCANO_GAP_MIN);
            } else {
                v.erupting = true;
                v.age = 0.0f;
                v.timer = VOLCANO_BURST;
            }
        }
        if (v.erupting) {
            v.age += dt;
            if ((int)parts_.size() < VOLCANO_MAX_PARTS && (rand() % 2) == 0) emit(v);
        }
    }

    for (int i = 0; i < (int)parts_.size();) {
        Particle &p = parts_[i];
        p.life -= dt;
        if (p.life <= 0.0f) {
            parts_.erase(parts_.begin() + i);
        } else {
            p.vy += VOLCANO_PART_GRAV * dt;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            i++;
        }
    }
}

void Volcanoes::draw(Renderer &r, float viewX, float viewY, float viewScale) const
{
    if (!enabled_) return;

    // Show at most VOLCANO_MAX_VISIBLE volcanoes per viewport: cull the ones
    // fully off screen (with margin for the lava arms and fireballs) and, if
    // several remain in view, keep those nearest the screen center (the
    // player's area of interest).
    int shown[VOLCANO_MAX_VISIBLE];
    int n = pickVisible(viewX, viewScale, shown);

    for (int k = 0; k < n; k++) {
        const Volcano &v = volc_[shown[k]];

        float pulse = 0.72f + 0.28f * sinf(t_ * 2.6f + v.phase);

        // Lava always pours downhill: one thick glowing ribbon per side of the peak.
        drawFlowArm(r, v.flowX, v.flowY, pulse, viewX, viewY, viewScale);
        drawFlowArm(r, v.flowX2, v.flowY2, pulse, viewX, viewY, viewScale);

        int sx = (int)roundf(v.x * viewScale + viewX);
        int sy = (int)roundf(v.gy * viewScale + viewY);

        // Sunken, irregular crater mouth: a jagged darker lip (recess) around the vent.
        for (int a = 0; a < 14; a++) {
            float ang = (float)a / 14.0f * 6.2832f;
            float rr = 2.6f + 0.9f * sinf(ang * 3.0f + v.phase * 5.0f);
            int lx = sx + (int)roundf(cosf(ang) * rr);
            int ly = sy - 1 + (int)roundf(sinf(ang) * rr * 0.8f);
            int b = (int)(85.0f + 25.0f * sinf(ang * 2.0f + t_ * 3.0f));
            r.pixelShade((float)lx, (float)ly, b);
        }

        // Glowing pulsing lava pool inside the crater mouth.
        float glow = 170.0f * pulse;
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                float d = sqrtf((float)(dx * dx + dy * dy));
                int b = (int)(glow * (1.0f - d / 3.5f));
                if (b >= 8) r.pixelShade((float)(sx + dx), (float)(sy - 1 + dy), b);
            }
        }
        int core = (int)(255.0f * (0.6f + 0.4f * pulse));
        r.pixelShade((float)sx, (float)(sy - 1), core);
        r.pixelShade((float)sx - 1, (float)(sy - 1), core / 2);
        r.pixelShade((float)sx + 1, (float)(sy - 1), core / 2);
        r.pixelShade((float)sx, (float)(sy - 2), core / 2);

        if (!v.erupting) continue;

        float flash = (v.age < VOLCANO_FLASH) ? (1.0f - v.age / VOLCANO_FLASH) : 0.0f;
        if (flash > 0.0f) {
            int rad = 5;
            for (int dy = -rad; dy <= rad; dy++) {
                for (int dx = -rad; dx <= rad; dx++) {
                    if (dx * dx + dy * dy > rad * rad) continue;
                    int b = (int)(245.0f * flash * (1.0f - sqrtf((float)(dx * dx + dy * dy)) / (float)(rad + 1)));
                    r.pixelShade((float)(sx + dx), (float)(sy - 1 + dy), b);
                }
            }
        }
    }

    // Eruption fireballs: bigger particles drawn as a small plus (wider area).
    for (int j = 0; j < (int)parts_.size(); j++) {
        const Particle &p = parts_[j];
        float lf = clampf(p.life / p.maxLife, 0.0f, 1.0f);
        int b = (int)(40.0f + 190.0f * lf);
        int bx = (int)roundf(p.x * viewScale + viewX);
        int by = (int)roundf(p.y * viewScale + viewY);
        r.pixelShade((float)bx, (float)by, b);
        r.pixelShade((float)(bx - 1), (float)by, b / 2);
        r.pixelShade((float)(bx + 1), (float)by, b / 2);
        r.pixelShade((float)bx, (float)(by - 1), b / 2);
        r.pixelShade((float)bx, (float)(by + 1), b / 2);
    }
}

int Volcanoes::pickVisible(float viewX, float viewScale, int *out) const
{
    const int margin = (int)(80.0f * viewScale);
    const int cx = (int)(SCREEN_W * 0.5f);
    int dist[VOLCANO_MAX_VISIBLE];
    int n = 0;
    for (int i = 0; i < (int)volc_.size(); i++) {
        int sx = (int)roundf(volc_[i].x * viewScale + viewX);
        if (sx < -margin || sx > (int)SCREEN_W + margin) continue;
        int d = sx < cx ? cx - sx : sx - cx;
        if (n < VOLCANO_MAX_VISIBLE) {
            out[n] = i;
            dist[n] = d;
            n++;
        } else {
            int worst = 0;
            for (int k = 1; k < n; k++) if (dist[k] > dist[worst]) worst = k;
            if (d < dist[worst]) {
                out[worst] = i;
                dist[worst] = d;
            }
        }
    }
    return n;
}

int Volcanoes::countInView(float viewX, float viewScale) const
{
    if (!enabled_) return 0;
    int shown[VOLCANO_MAX_VISIBLE];
    return pickVisible(viewX, viewScale, shown);
}
