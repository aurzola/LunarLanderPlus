#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cstdio>
#include "terrain.h"
#include "renderer.h"
#include "config.h"

Terrain::Terrain()
{
}

float Terrain::midpoint(float p1, float p2)
{
    return (p1 + p2) / 2.f;
}

void Terrain::generate(int width, int height, float displacement, int iteration)
{
    float midx, midy;
    if (iteration > 7) {
        multiplierPlace();
        points();
        return;
    }

    static bool seeded = false;
    if (iteration == 1) {
        x.clear();
        y.clear();
        if (!seeded) {
            srand((unsigned)time(NULL));
            seeded = true;
        }
        x.push_back(0);
        x.push_back((float)width);
        y.push_back(4.f / 5.f * height);
        y.push_back(4.f / 5.f * height);
    } else {
        for (int i = 1; i < (int)x.size(); i++) {
            midx = midpoint(x[i], x[i - 1]);
            midy = midpoint(y[i], y[i - 1]);
            midy += ((float)rand() / RAND_MAX) * displacement - displacement / 2.f;
            if (midy > height - 25) midy = (float)(height - 25);
            xm.push_back(midx);
            ym.push_back(midy);
        }
        xp = x;
        yp = y;
        x.clear();
        y.clear();
        for (int i = 0; i < (int)xp.size(); i++) {
            x.push_back(xp[i]);
            y.push_back(yp[i]);
            if (i < (int)xp.size() - 1) {
                x.push_back(xm[i]);
                y.push_back(ym[i]);
            }
        }
        xm.clear();
        ym.clear();
    }
    generate(width, height, displacement * 2.f / 3.f, iteration + 1);
}

void Terrain::draw(Renderer &r)
{
    float kx = SCREEN_W / WORLD_W;
    float ky = SCREEN_H / WORLD_H;
    char buf[8];

    for (int i = 1; i < (int)x.size(); i++) {
        r.line(x[i - 1] * kx, y[i - 1] * ky, x[i] * kx, y[i] * ky);
    }

    for (int i = 0; i < (int)multipliersIndexes.size(); i++) {
        int a = multipliersIndexes[i];
        int b = multipliersIndexes[i] + multipliersLengths[i] - 1;
        r.line(x[a] * kx, (y[a] - 1) * ky, x[b] * kx, (y[b] - 1) * ky);
        r.line(x[a] * kx, (y[a] - 2) * ky, x[b] * kx, (y[b] - 2) * ky);
        snprintf(buf, sizeof buf, "%dx", multipliersValues[i]);
        r.text((x[a] + x[b]) / 2.f * kx, (y[a] + 20) * ky, buf);
    }
}

void Terrain::points()
{
    float slope, yNew, yPrev;
    xPoints.clear();
    yPoints.clear();
    for (int i = 1; i < (int)x.size(); i++) {
        slope = (y[i] - y[i - 1]) / (x[i] - x[i - 1]);
        yPrev = y[i - 1];
        for (float j = x[i - 1]; j < x[i]; j++) {
            yNew = slope + yPrev;
            xPoints.push_back((int)roundf(j));
            yPoints.push_back((int)roundf(yNew));
            yPrev = yNew;
        }
    }
}

std::vector<int> Terrain::getXPoints() { return xPoints; }
std::vector<int> Terrain::getYPoints() { return yPoints; }

void Terrain::multiplierPlace()
{
    int length, place;
    bool overlapped;
    multipliersLengths.clear();
    multipliersIndexes.clear();
    multipliersValues.clear();

    for (int i = 0; i < 4; i++) {
        length = rand() % 4 + 4;
        place = rand() % ((int)x.size() - length);

        do {
            overlapped = false;
            for (int k = 0; k < (int)multipliersIndexes.size(); k++) {
                for (int l = 0; l < multipliersLengths[k]; l++) {
                    for (int m = 0; m < length; m++) {
                        if (x[place + m] == x[multipliersIndexes[k] + l]) {
                            overlapped = true;
                            length = rand() % 4 + 4;
                            place = rand() % ((int)x.size() - length);
                            break;
                        }
                    }
                    if (overlapped) break;
                }
                if (overlapped) break;
            }
        } while (overlapped);

        switch (length) {
            case 4: multipliersValues.push_back(5); break;
            case 5: multipliersValues.push_back(4); break;
            case 6: multipliersValues.push_back(3); break;
            case 7: multipliersValues.push_back(2); break;
            default: break;
        }

        multipliersLengths.push_back(length);
        multipliersIndexes.push_back(place);

        for (int j = 0; j < length; j++) {
            y[place + j] = y[place];
        }
    }
}

int Terrain::multiplierCheck(int xpos)
{
    for (int i = 0; i < (int)multipliersIndexes.size(); i++) {
        if (xpos >= x[multipliersIndexes[i]] &&
            xpos <= x[multipliersIndexes[i] + multipliersLengths[i] - 1]) {
            return multipliersValues[i];
        }
    }
    return 1;
}
