#ifndef TERRAIN_H
#define TERRAIN_H

#include <vector>
#include <utility>

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

struct ZoneInfo {
    int startIdx;
    int segCount;
    float labelX;
    bool broken;
    float baseY;
};

class Terrain {
public:
    Terrain();

    void init();
    void generate(int level);
    void draw(Renderer &r, float viewX, float viewY, float viewScale, int counter,
              bool drawStars = true, bool withLabels = true);
    void drawLabels(Renderer &r, float viewX, float viewY, float viewScale);
    void drawStarField(Renderer &r, float viewX, float viewY, float viewScale);
    const std::vector<TerrainLine>& getLines() const { return lines; }
    float getWidth() const { return tileWidth; }
    int checkLanding(float left, float right, float bottom, float rotation, float vy, float vx);
    float yAt(float x, float fallback = 500.0f) const;
    void setCrater(float x, float halfW);
    void clearCrater();
    bool hasCrater() const { return craterActive; }
    unsigned revision() const { return revision_; }
    bool onChuteSpot(float x) const { return x >= chuteZoneX1 && x <= chuteZoneX2; }
    float chuteLabel() const { return chuteLabelX; }

    int zoneCount() const { return (int)zones_.size(); }
    int zoneStart(int z) const { return (z >= 0 && z < (int)zones_.size()) ? zones_[z].startIdx : -1; }
    int zoneSegCount(int z) const { return (z >= 0 && z < (int)zones_.size()) ? zones_[z].segCount : 0; }
    float zoneLabelX(int z) const { return (z >= 0 && z < (int)zones_.size()) ? zones_[z].labelX : -1.0f; }
    bool zoneBroken(int z) const { return (z >= 0 && z < (int)zones_.size()) ? zones_[z].broken : false; }
    int zoneOverlapping(float x1, float x2) const;
    void ruptureZone(int zone);
    void ruptureSurface(float cx, float halfW);
    bool isZoneRupturedAt(float x) const;
    bool isRupturedAt(float x) const;

private:
    std::vector<TerrainLine> lines;
    std::vector<ZoneInfo> zones_;
    std::vector<Star> stars;
    std::vector<std::pair<float, float> > ruptureRanges_;
    float tileWidth;
    float chuteZoneX1, chuteZoneX2, chuteLabelX;
    bool craterActive;
    float craterX, craterHalfW;
    unsigned revision_;
    void addLine(float x1, float y1, float x2, float y2);
};

#endif
