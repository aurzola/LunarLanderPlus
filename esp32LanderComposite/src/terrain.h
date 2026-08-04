#ifndef TERRAIN_H
#define TERRAIN_H

#include <vector>

class Renderer;

class Terrain {
public:
    Terrain();

    float midpoint(float p1, float p2);
    void generate(int width, int height, float displacement, int iteration);
    void draw(Renderer &r);
    void points();
    std::vector<int> getXPoints();
    std::vector<int> getYPoints();
    void multiplierPlace();
    int multiplierCheck(int xpos);

private:
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> xm;
    std::vector<float> ym;
    std::vector<float> xp;
    std::vector<float> yp;
    std::vector<int> multipliersValues;
    std::vector<int> multipliersLengths;
    std::vector<int> multipliersIndexes;
    std::vector<int> xPoints;
    std::vector<int> yPoints;
};

#endif
