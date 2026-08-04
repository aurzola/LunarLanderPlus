#ifndef TERRAIN_H
#define TERRAIN_H

#include <vector>

class Renderer;

struct TerrainLine {
    float x1, y1, x2, y2;
    bool landable;
    int multiplier;
    float labelX;
};

struct Star {
    float x, y;
};

class Terrain {
public:
    Terrain();

    void init();
    void draw(Renderer &r, float viewX, float viewY, float viewScale, int counter);
    const std::vector<TerrainLine>& getLines() const { return lines; }
    float getWidth() const { return tileWidth; }
    int checkLanding(float left, float right, float bottom, float rotation, float vy, float vx);

private:
    std::vector<TerrainLine> lines;
    std::vector<Star> stars;
    float tileWidth;
    void addLine(float x1, float y1, float x2, float y2);
};

#endif
