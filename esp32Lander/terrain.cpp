#include <cstdlib>
#include <cmath>
#include <cstdio>
#include "terrain.h"
#include "renderer.h"
#include "config.h"
#include "moons.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

Terrain::Terrain() : tileWidth(0), chuteZoneX1(0), chuteZoneX2(0), chuteLabelX(-1),
                     craterActive(false), craterX(0), craterHalfW(0), revision_(0) {}

void Terrain::setCrater(float x, float halfW)
{
    craterActive = true;
    revision_++;
    craterX = x;
    craterHalfW = halfW;
}

void Terrain::clearCrater()
{
    if (craterActive) { craterActive = false; revision_++; }
}

void Terrain::addLine(float x1, float y1, float x2, float y2)
{
    TerrainLine l;
    l.x1 = x1; l.y1 = y1;
    l.x2 = x2; l.y2 = y2;
    l.landable = (y1 == y2);
    l.multiplier = 1;
    l.labelX = -1;
    lines.push_back(l);
}

void Terrain::init()
{
    revision_++;
    lines.clear();
    stars.clear();
    ruptureRanges_.clear();

    const float S = 1.35f;
    const float OY = 130.0f;

    float pts[][2] = {
        {0,355},{5,355},{6,359},{11,359},{12,364},{15,364},{16,376},{19,388},
        {19,392},{22,400},{29,404},{31,412},{33,417},{38,421},{43,421},
        {47,417},{52,410},{57,404},{61,400},{64,396},{68,392},{70,388},
        {75,386},{80,380},{85,379},{89,376},{94,376},{99,377},{103,380},
        {104,384},{108,388},{109,392},{112,396},{113,400},{117,404},
        {122,404},{125,396},{129,394},{132,396},{136,400},{138,408},
        {145,412},{146,425},{150,437},{150,441},{154,445},{163,445},
        {168,441},{173,437},{175,433},{180,429},{182,425},{187,423},
        {189,412},{192,404},{196,402},{201,398},{205,392},{210,384},
        {213,376},{217,372},{219,368},{221,364},{224,359},{229,359},
        {234,356},{238,348},{243,343},{245,335},{247,323},{247,315},
        {248,307},{252,297},{257,295},{258,290},{261,286},{266,286},
        {267,290},{272,290},{273,295},{275,295},{279,297},{282,300},
        {285,308},{292,313},{299,331},{303,332},{308,335},{309,339},
        {312,343},{314,347},{317,351},{322,351},{323,364},{327,376},
        {327,380},{331,380},{332,384},{336,388},{338,396},{340,400},
        {345,404},{346,417},{350,429},{350,433},{351,437},{354,441},
        {359,441},{361,449},{364,453},{368,457},{373,461},{410,461},
        {413,449},{417,441},{420,433},{422,433},{425,425},{429,422},
        {433,417},{438,415},{443,412},{447,412},{449,417},{455,431},
        {456,435},{459,439},{463,441},{466,445},{468,453},{476,457},
        {485,457},{495,458},{504,461},{522,461},{525,453},{527,441},
        {527,433},{532,433},{534,425},{539,421},{541,417},{542,413},
        {546,408},{550,408},{553,398},{555,390},{560,388},{564,392},
        {573,392},{578,388},{580,380},{583,369},{588,368},{589,364},
        {592,360},{597,356}
    };

    int n = sizeof(pts) / sizeof(pts[0]);
    tileWidth = pts[n - 1][0] * S;

    for (int i = 0; i < n - 1; i++) {
        float x1 = pts[i][0] * S;
        float y1 = pts[i][1] * S + OY;
        float x2 = pts[i + 1][0] * S;
        float y2 = pts[i + 1][1] * S + OY;
        addLine(x1, y1, x2, y2);
    }
    addLine(pts[n - 1][0] * S, pts[n - 1][1] * S + OY,
            pts[0][0] * S + tileWidth, pts[0][1] * S + OY);

    static const int landingIdx[] = {34, 63, 106, 133};
    static const int landingMul[] = {4, 5, 5, 2};
    for (int i = 0; i < 4; i++) {
        int idx = landingIdx[i];
        // Pad width varies with score multiplier: higher multiplier = narrower
        // (harder) pad. 5x->3, 4x->4, 2x->5 lines. The narrowest pad (idx 106,
        // ~14u) still fits the ship box (~9.6u at 5x zoom) with margin.
        int segs = (landingMul[i] == 5) ? 3 : (landingMul[i] == 2 ? 5 : 4);
        float ly = lines[idx].y1;
        for (int k = idx; k < idx + segs && k < (int)lines.size(); k++) {
            lines[k].y1 = ly;
            lines[k].y2 = ly;
            lines[k].landable = true;
            lines[k].multiplier = landingMul[i];
        }
        int last = idx + segs - 1 < (int)lines.size() ? idx + segs - 1 : (int)lines.size() - 1;
        float zoneCenterX = (lines[idx].x1 + lines[last].x2) / 2.0f;
        lines[idx].labelX = zoneCenterX;
    }

    // Populate zone registry so other systems can query and rupture pads.
    zones_.clear();
    for (int i = 0; i < 4; i++) {
        int idx = landingIdx[i];
        int segs = (landingMul[i] == 5) ? 3 : (landingMul[i] == 2 ? 5 : 4);
        ZoneInfo zi;
        zi.startIdx = idx;
        zi.segCount = segs;
        zi.labelX = lines[idx].labelX;
        zi.broken = false;
        zi.baseY = lines[idx].y1;
        zones_.push_back(zi);
    }

    chuteLabelX = lines[landingIdx[0]].labelX;
    chuteZoneX1 = lines[landingIdx[0]].x1;
    chuteZoneX2 = lines[landingIdx[0] + 3].x2;

    float terrainTop = 9999;
    for (int i = 0; i < (int)lines.size(); i++) {
        if (lines[i].y1 < terrainTop) terrainTop = lines[i].y1;
    }

    for (int i = 0; i < MAX_STARS; i++) {
        Star s;
        s.x = (float)(rand() % (int)tileWidth);
        s.y = (float)(rand() % (int)(terrainTop - 30.0f)) + 15.0f;
        stars.push_back(s);
    }
}

void Terrain::generate(int level)
{
    revision_++;
    lines.clear();
    stars.clear();
    ruptureRanges_.clear();

    const float S = 1.35f;
    const float OY = 130.0f;

    const int NP = 150;
    std::vector<float> px(NP), py(NP);

    px[0] = 0.0f;
    py[0] = 390.0f + (float)(rand() % 60);

    int amp = (level < 8) ? (4 + level) : 12;
    float drift = 0.0f;
    float phase = (float)(rand() % 628) / 100.0f;
    float freq = (0.008f + (float)(rand() % 8) * 0.001f) * 4.0f;

    for (int i = 1; i < NP; i++) {
        px[i] = px[i - 1] + 3.0f + (float)(rand() % 4);
        drift += (float)(rand() % (2 * amp + 1)) - amp;
        if (drift > 40) drift = 40;
        if (drift < -40) drift = -40;
        py[i] = 420.0f + drift + 18.0f * sinf(px[i] * freq + phase);
        if (py[i] < 340) py[i] = 340;
        if (py[i] > 500) py[i] = 500;
    }

    for (int pass = 0; pass < 2; pass++) {
        for (int i = 1; i < NP - 1; i++) {
            py[i] = (py[i - 1] + py[i] + py[i + 1]) / 3.0f;
        }
    }

    static const int landingMul[] = {4, 5, 5, 2};
    // Pad width varies with score multiplier: higher multiplier = narrower
    // (harder) pad. Lines per pad: mult4->5, mult5->4, mult2->6. Ganymede
    // lands at 2x zoom (ship box ~24u) so its pads are 2 lines wider; other
    // moons land at 5x (box ~9.6u).
    bool ganymede = moonHasRings(level);
    static const int padLines[] = {5, 4, 4, 6};
    int zoneStart[4];
    for (int j = 0; j < 4; j++) {
        int segs = padLines[j] + (ganymede ? 2 : 0);
        zoneStart[j] = (NP - 20) * j / 4 + (rand() % 8);
        float zy = 0;
        for (int k = zoneStart[j]; k <= zoneStart[j] + segs; k++) zy += py[k];
        zy /= (float)(segs + 1);
        for (int k = zoneStart[j]; k <= zoneStart[j] + segs; k++) py[k] = zy;
    }

    tileWidth = px[NP - 1] * S;

    for (int i = 0; i < NP - 1; i++) {
        addLine(px[i] * S, py[i] * S + OY, px[i + 1] * S, py[i + 1] * S + OY);
    }
    addLine(px[NP - 1] * S, py[NP - 1] * S + OY,
            px[0] * S + tileWidth, py[0] * S + OY);

    int li = 0;
    int firstZoneIdx = -1;
    zones_.clear();
    for (int j = 0; j < 4; j++) {
        int segs = padLines[j] + (ganymede ? 2 : 0);
        while (li < zoneStart[j]) li++;
        int idx = li;
        if (j == 0) firstZoneIdx = idx;
        float zoneCenterX = (lines[idx].x1 + lines[idx + segs - 1].x2) / 2.0f;
        for (int k = idx; k < idx + segs; k++) {
            lines[k].multiplier = landingMul[j];
        }
        lines[idx].labelX = zoneCenterX;

        ZoneInfo zi;
        zi.startIdx = idx;
        zi.segCount = segs;
        zi.labelX = zoneCenterX;
        zi.broken = false;
        zi.baseY = lines[idx].y1;
        zones_.push_back(zi);
    }

    chuteLabelX = lines[firstZoneIdx].labelX;
    chuteZoneX1 = lines[firstZoneIdx].x1;
    chuteZoneX2 = lines[firstZoneIdx + padLines[0] + (ganymede ? 1 : 0)].x2;

    float terrainTop = 9999;
    for (int i = 0; i < (int)lines.size(); i++) {
        if (lines[i].y1 < terrainTop) terrainTop = lines[i].y1;
    }

    for (int i = 0; i < MAX_STARS; i++) {
        Star s;
        s.x = (float)(rand() % (int)tileWidth);
        s.y = (float)(rand() % (int)(terrainTop - 30.0f)) + 15.0f;
        stars.push_back(s);
    }
}

void Terrain::drawLabels(Renderer &r, float viewX, float viewY, float viewScale)
{
    for (int i = 0; i < (int)lines.size(); i++) {
        const TerrainLine &l = lines[i];
        if (!l.landable || l.multiplier <= 1 || l.labelX < 0) continue;
        char buf[16];
        snprintf(buf, sizeof buf, "%dx", l.multiplier);
        float mx = l.labelX * viewScale + viewX;
        float my = (l.y1 + 10.0f) * viewScale + viewY;
        r.text(mx - 6, my, buf);
        if (l.labelX == chuteLabelX) r.text(mx - 3, my + 8, "p");
    }
}

void Terrain::drawStarField(Renderer &r, float viewX, float viewY, float viewScale)
{
    const int RW = r.width(), RH = r.height();
    for (int i = 0; i < (int)stars.size(); i++) {
        float sx = stars[i].x * viewScale + viewX;
        float sy = stars[i].y * viewScale + viewY;
        if (sx < -5 || sx > RW + 5 || sy < -5 || sy > RH + 5) continue;
        r.rect(sx, sy, 1, 1);
    }
}

void Terrain::draw(Renderer &r, float viewX, float viewY, float viewScale, int /*counter*/,
                   bool drawStars, bool withLabels)
{
    const int RW = r.width();
    float c1 = craterActive ? craterX - craterHalfW : 0.0f;
    float c2 = craterActive ? craterX + craterHalfW : 0.0f;

    auto interp = [](float x1, float y1, float x2, float y2, float x) {
        if (x2 == x1) return y1;
        float t = (x - x1) / (x2 - x1);
        return y1 + (y2 - y1) * t;
    };

    auto drawSeg = [&](float x1, float y1, float x2, float y2, const TerrainLine &l) {
        if (x2 < x1) { float t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
        float sx1 = x1 * viewScale + viewX;
        float sy1 = y1 * viewScale + viewY;
        float sx2 = x2 * viewScale + viewX;
        float sy2 = y2 * viewScale + viewY;
        if (sx2 < -10 || sx1 > RW + 10) return;
        r.line(sx1, sy1, sx2, sy2);
        if (l.landable && l.multiplier > 1) {
            r.line(sx1, sy1 - 1, sx2, sy2 - 1);
        }
    };

    for (int i = 0; i < (int)lines.size(); i++) {
        const TerrainLine &l = lines[i];

        if (craterActive && l.x2 > c1 && l.x1 < c2) {
            if (l.x1 >= c1 && l.x2 <= c2) {
                // Whole segment erased by the crater: open gap in the surface.
                continue;
            }
            if (l.x1 < c1) drawSeg(l.x1, l.y1, c1, interp(l.x1, l.y1, l.x2, l.y2, c1), l);
            if (l.x2 > c2) drawSeg(c2, interp(l.x1, l.y1, l.x2, l.y2, c2), l.x2, l.y2, l);
        } else {
            drawSeg(l.x1, l.y1, l.x2, l.y2, l);
        }

        if (i + 1 < (int)lines.size()) {
            const TerrainLine &n = lines[i + 1];
            if (l.x2 == n.x1 && l.y2 != n.y1) {
                r.line(l.x2 * viewScale + viewX, l.y2 * viewScale + viewY,
                       n.x1 * viewScale + viewX, n.y1 * viewScale + viewY);
            }
        }
    }

    if (withLabels) drawLabels(r, viewX, viewY, viewScale);

    if (drawStars) drawStarField(r, viewX, viewY, viewScale);
}

int Terrain::checkLanding(float left, float right, float bottom, float rotation, float vy, float /*vx*/)
{
    for (int i = 0; i < (int)lines.size(); i++) {
        const TerrainLine &l = lines[i];
        if (right < l.x1 || left > l.x2) continue;

        if (l.landable) {
            if (bottom >= l.y1) {
                int rs = i, re = i;
                while (rs > 0 && lines[rs - 1].landable) rs--;
                while (re < (int)lines.size() - 1 && lines[re + 1].landable) re++;
                float zx1 = lines[rs].x1, zx2 = lines[re].x2;
                bool inside = (left > zx1 && right < zx2);
                bool good = (fabsf(rotation) <= LAND_MAX_ROTATION && vy < LAND_HARD_VY);
                int res = (inside && good) ? 2 : 1;
#if defined(ARDUINO)
                Serial.printf("[land] zone i=%d..%d x1=%.1f x2=%.1f L=%.1f R=%.1f B=%.1f rot=%.1f vy=%.3f inside=%d good=%d -> %d\n",
                              rs, re, zx1, zx2, left, right, bottom, rotation, vy, inside, good, res);
#endif
                return res;
            }
        } else {
            if (bottom > l.y1 || bottom > l.y2) {
                float dist = (left - l.x1) / (l.x2 - l.x1);
                if (dist > 0 && dist < 1) {
                    float yhit = l.y1 + (l.y2 - l.y1) * dist;
                    if (yhit <= bottom) {
#if defined(ARDUINO)
                        Serial.printf("[land] i=%d ROCK x1=%.1f x2=%.1f y1=%.1f y2=%.1f L=%.1f R=%.1f B=%.1f rot=%.1f vy=%.3f yhit=%.1f -> 1\n",
                                      i, l.x1, l.x2, l.y1, l.y2, left, right, bottom, rotation, vy, yhit);
#endif
                        return 1;
                    }
                }
                dist = (right - l.x1) / (l.x2 - l.x1);
                if (dist > 0 && dist < 1) {
                    float yhit = l.y1 + (l.y2 - l.y1) * dist;
                    if (yhit <= bottom) {
#if defined(ARDUINO)
                        Serial.printf("[land] i=%d ROCK x1=%.1f x2=%.1f y1=%.1f y2=%.1f L=%.1f R=%.1f B=%.1f rot=%.1f vy=%.3f yhit=%.1f -> 1\n",
                                      i, l.x1, l.x2, l.y1, l.y2, left, right, bottom, rotation, vy, yhit);
#endif
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

float Terrain::yAt(float x, float fallback) const
{
    for (int i = 0; i < (int)lines.size(); i++) {
        const TerrainLine &l = lines[i];
        if (x >= l.x1 && x <= l.x2 && l.x2 != l.x1) {
            float t = (x - l.x1) / (l.x2 - l.x1);
            return l.y1 + (l.y2 - l.y1) * t;
        }
    }
    return fallback;
}

void Terrain::ruptureZone(int zone)
{
    if (zone < 0 || zone >= (int)zones_.size()) return;
    ZoneInfo &zi = zones_[zone];
    if (zi.broken) return;
    zi.broken = true;
    zi.labelX = -1;
    revision_++;

    int s = zi.startIdx;
    int n = zi.segCount;
    float baseY = zi.baseY;

    for (int k = 0; k < n; k++) {
        int vi = s + k;
        if (vi >= (int)lines.size()) break;
        float t0 = (float)k / (float)n;
        float t1 = (float)(k + 1) / (float)n;
        float lift0 = QUAKE_LIFT * sinf(t0 * (float)M_PI);
        float lift1 = QUAKE_LIFT * sinf(t1 * (float)M_PI);
        lines[vi].y1 = baseY + lift0;
        lines[vi].y2 = baseY + lift1;
        lines[vi].landable = false;
        lines[vi].multiplier = 1;
    }
    lines[s].labelX = -1;
}

int Terrain::zoneOverlapping(float x1, float x2) const
{
    for (int i = 0; i < (int)zones_.size(); i++) {
        int s = zones_[i].startIdx;
        int n = zones_[i].segCount;
        if (s + n - 1 >= (int)lines.size()) continue;
        float zx1 = lines[s].x1;
        float zx2 = lines[s + n - 1].x2;
        if (x2 >= zx1 && x1 <= zx2) return i;
    }
    return -1;
}

void Terrain::ruptureSurface(float cx, float halfW)
{
    revision_++;
    float w1 = cx - halfW;
    float w2 = cx + halfW;

    // Any landing pad touched by the window is destroyed entirely: its flat
    // surface buckles, becomes non-landable and loses its label (which also
    // clears the approach lights and the minimap point).
    for (int z = 0; z < (int)zones_.size(); z++) {
        int s = zones_[z].startIdx;
        int n = zones_[z].segCount;
        if (s + n - 1 >= (int)lines.size()) continue;
        float zx1 = lines[s].x1;
        float zx2 = lines[s + n - 1].x2;
        if (w2 >= zx1 && w1 <= zx2) ruptureZone(z);
    }

    // Buckle the open surface itself: every non-zone segment in the window
    // is lifted by a hump whose peak depends on where the segment sits in it.
    for (int i = 0; i < (int)lines.size(); i++) {
        if (lines[i].x2 <= w1 || lines[i].x1 >= w2) continue;
        bool inZone = false;
        for (int z = 0; z < (int)zones_.size() && !inZone; z++) {
            int s = zones_[z].startIdx;
            int n = zones_[z].segCount;
            if (i >= s && i < s + n) inZone = true;
        }
        if (inZone) continue;
        float lo = (lines[i].x1 > w1) ? lines[i].x1 : w1;
        float hi = (lines[i].x2 < w2) ? lines[i].x2 : w2;
        float mid = (lo + hi) * 0.5f;
        float t = (mid - w1) / (w2 - w1);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float lift = QUAKE_LIFT * sinf(t * (float)M_PI);
        lines[i].y1 += lift;
        lines[i].y2 += lift;
        lines[i].landable = false;
    }

    ruptureRanges_.push_back(std::make_pair(w1, w2));
}

bool Terrain::isRupturedAt(float x) const
{
    if (isZoneRupturedAt(x)) return true;
    for (int i = 0; i < (int)ruptureRanges_.size(); i++) {
        if (x >= ruptureRanges_[i].first && x <= ruptureRanges_[i].second) return true;
    }
    return false;
}

bool Terrain::isZoneRupturedAt(float x) const
{
    for (int i = 0; i < (int)zones_.size(); i++) {
        if (!zones_[i].broken) continue;
        int s = zones_[i].startIdx;
        int n = zones_[i].segCount;
        if (s + n - 1 >= (int)lines.size()) continue;
        float zx1 = lines[s].x1;
        float zx2 = lines[s + n - 1].x2;
        if (x >= zx1 && x <= zx2) return true;
    }
    return false;
}
