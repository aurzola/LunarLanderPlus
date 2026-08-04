#include <cstdlib>
#include <cmath>
#include <cstdio>
#include "terrain.h"
#include "renderer.h"
#include "config.h"

Terrain::Terrain() : tileWidth(0) {}

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
    lines.clear();
    stars.clear();

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
        float ly = lines[idx].y1;
        for (int k = idx; k < idx + 4 && k < (int)lines.size(); k++) {
            lines[k].y1 = ly;
            lines[k].y2 = ly;
            lines[k].landable = true;
            lines[k].multiplier = landingMul[i];
        }
        int last = idx + 3 < (int)lines.size() ? idx + 3 : (int)lines.size() - 1;
        float zoneCenterX = (lines[idx].x1 + lines[last].x2) / 2.0f;
        lines[idx].labelX = zoneCenterX;
    }

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

void Terrain::draw(Renderer &r, float viewX, float viewY, float viewScale, int /*counter*/)
{
    for (int i = 0; i < (int)lines.size(); i++) {
        const TerrainLine &l = lines[i];
        float sx1 = l.x1 * viewScale + viewX;
        float sy1 = l.y1 * viewScale + viewY;
        float sx2 = l.x2 * viewScale + viewX;
        float sy2 = l.y2 * viewScale + viewY;

        if (sx2 < -10 || sx1 > SCREEN_W + 10) continue;

        r.line(sx1, sy1, sx2, sy2);

        if (l.landable && l.multiplier > 1) {
            r.line(sx1, sy1 - 1, sx2, sy2 - 1);
            if (l.labelX >= 0) {
                char buf[8];
                snprintf(buf, sizeof buf, "%dx", l.multiplier);
                float mx = l.labelX * viewScale + viewX;
                float my = (l.y1 + 20.0f) * viewScale + viewY;
                r.text(mx - 6, my, buf);
            }
        }
    }

    for (int i = 0; i < (int)stars.size(); i++) {
        float sx = stars[i].x * viewScale + viewX;
        float sy = stars[i].y * viewScale + viewY;
        if (sx < -5 || sx > SCREEN_W + 5 || sy < -5 || sy > SCREEN_H + 5) continue;
        r.rect(sx, sy, 1, 1);
    }
}

int Terrain::checkLanding(float left, float right, float bottom, float rotation, float vy, float /*vx*/)
{
    for (int i = 0; i < (int)lines.size(); i++) {
        const TerrainLine &l = lines[i];
        if (right < l.x1 || left > l.x2) continue;

        if (l.landable) {
            if (bottom >= l.y1) {
                if (left > l.x1 && right < l.x2) {
                    if (rotation == 0 && vy < LAND_HARD_VY) {
                        return 2;
                    } else {
                        return 1;
                    }
                } else {
                    return 1;
                }
            }
        } else {
            if (bottom > l.y1 || bottom > l.y2) {
                float dist = (left - l.x1) / (l.x2 - l.x1);
                if (dist > 0 && dist < 1) {
                    float yhit = l.y1 + (l.y2 - l.y1) * dist;
                    if (yhit <= bottom) return 1;
                }
                dist = (right - l.x1) / (l.x2 - l.x1);
                if (dist > 0 && dist < 1) {
                    float yhit = l.y1 + (l.y2 - l.y1) * dist;
                    if (yhit <= bottom) return 1;
                }
            }
        }
    }
    return 0;
}
