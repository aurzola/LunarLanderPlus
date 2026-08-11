#include <cmath>
#include <cstdlib>
#include "acidrain.h"
#include "config.h"
#include "moons.h"

namespace {

// Deterministic pseudo-random in [0,1): never consumes the game RNG, so draw
// is stable frame-to-frame and does not change gameplay outcomes.
float prand(int a, int b)
{
    double s = sin((double)a * 12.9898 + (double)b * 78.233) * 43758.5453;
    return (float)(s - floor(s));
}

float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

} // namespace

AcidRain::AcidRain()
    : level_(1), enabled_(false), t_(0.0f), meter_(0.0f), worldW_(800.0f)
{
}

void AcidRain::reset(int level, const Terrain &t)
{
    level_ = level;
    enabled_ = moonHasAcidRain(level);
    meter_ = 0.0f;
    t_ = 0.0f;
    cells_.clear();
    worldW_ = t.getWidth();
    if (!enabled_) return;

    // Evenly spaced cells with jitter so they do not sit on top of each other.
    for (int i = 0; i < ACID_RAIN_CELLS; i++) {
        Cell c;
        float u = (float)(rand() % 10000) / 10000.0f;
        c.x = worldW_ * (i + 0.5f) / (float)ACID_RAIN_CELLS;
        c.x += (u - 0.5f) * 2.0f * ACID_CELL_RADIUS * 0.5f;
        c.x = clampf(c.x, ACID_CELL_RADIUS, worldW_ - ACID_CELL_RADIUS);
        c.dir = (rand() % 2) ? 1.0f : -1.0f;
        cells_.push_back(c);
    }
}

void AcidRain::update(float dt)
{
    t_ += dt;
    if (!enabled_) {
        cells_.clear();
        meter_ = 0.0f;
        return;
    }

    for (int i = 0; i < (int)cells_.size(); i++) {
        Cell &c = cells_[i];
        c.x += c.dir * ACID_CELL_DRIFT * dt;
        if (c.x < ACID_CELL_RADIUS) {
            c.x = ACID_CELL_RADIUS;
            c.dir = 1.0f;
        } else if (c.x > worldW_ - ACID_CELL_RADIUS) {
            c.x = worldW_ - ACID_CELL_RADIUS;
            c.dir = -1.0f;
        }
    }
}

bool AcidRain::inRain(float x, float y) const
{
    if (!enabled_) return false;
    if (y < ACID_CELL_TOP) return false;
    for (int i = 0; i < (int)cells_.size(); i++) {
        if (fabsf(x - cells_[i].x) < ACID_CELL_RADIUS) return true;
    }
    return false;
}

void AcidRain::corrode()
{
    meter_ += ACID_RAIN_CORRODE;
    if (meter_ > 100.0f) meter_ = 100.0f;
}

void AcidRain::dry()
{
    meter_ -= ACID_DRY_RATE;
    if (meter_ < 0.0f) meter_ = 0.0f;
}

void AcidRain::draw(Renderer &r, const Terrain &t, float viewX, float viewY,
                    float viewScale) const
{
    if (!enabled_) return;

    float vx0 = (-viewX) / viewScale;
    float vx1 = (SCREEN_W - viewX) / viewScale;
    float vy0 = (-viewY) / viewScale;
    float vy1 = (SCREEN_H - viewY) / viewScale;

    for (int i = 0; i < (int)cells_.size(); i++) {
        const Cell &c = cells_[i];
        float cx0 = c.x - ACID_CELL_RADIUS, cx1 = c.x + ACID_CELL_RADIUS;
        if (cx1 < vx0 - 10.0f || cx0 > vx1 + 10.0f) continue;

        for (int k = 0; k < ACID_RAIN_STREAKS; k++) {
            float u = prand(i * 1000 + k, 7);
            float sx = cx0 + (k + u) / (float)ACID_RAIN_STREAKS * (cx1 - cx0);
            sx += (u - 0.5f) * 4.0f;
            float y = ACID_CELL_TOP + fmodf(u * ACID_STREAK_CYCLE + t_ * ACID_FALL_SPEED,
                                            ACID_STREAK_CYCLE);
            if (y < vy0 - 10.0f || y > vy1 + 10.0f) continue;
            float dx = -ACID_STREAK_SLANT * ACID_STREAK_LEN;
            float yBot = y + ACID_STREAK_LEN;
            float ter = t.yAt(sx + dx * 0.5f, 600.0f);
            if (yBot > ter) yBot = ter;
            if (yBot <= y) continue;
            int b = 120 + (int)(60.0f * u);
            r.lineShade(sx * viewScale + viewX, y * viewScale + viewY,
                        (sx + dx) * viewScale + viewX,
                        yBot * viewScale + viewY, b);
        }

        // Faint splashes where the rain hits the ground.
        for (int j = 0; j < 3; j++) {
            float u = prand(i * 3 + j, 13);
            float sx = cx0 + u * (cx1 - cx0);
            if (sx < vx0 || sx > vx1) continue;
            float gy = t.yAt(sx, 500.0f);
            float pulse = 0.7f + 0.3f * sinf(t_ * 3.0f + (float)j + (float)i);
            int b = (int)(20.0f + 50.0f * pulse);
            int px = (int)roundf(sx * viewScale + viewX);
            int py = (int)roundf(gy * viewScale + viewY);
            r.pixelShade((float)px, (float)py, b);
            r.pixelShade((float)(px - 1), (float)py, b / 2);
            r.pixelShade((float)(px + 1), (float)py, b / 2);
            r.pixelShade((float)px, (float)(py - 1), b / 2);
            r.pixelShade((float)px, (float)(py + 1), b / 2);
        }
    }
}

void AcidRain::drawSizzle(Renderer &r, float shipX, float shipY, float viewX,
                          float viewY, float viewScale) const
{
    // Thin steam puffs rising off the hull while the acid eats the ship.
    for (int k = 0; k < 3; k++) {
        float off = fmodf(t_ * 12.0f + (float)k * 6.0f, 12.0f);
        float px = shipX + ((float)k - 1.0f) * 3.0f + sinf(t_ * 2.0f + (float)k * 2.0f) * 1.5f;
        float py = shipY - 10.0f - off;
        int b = 110 - (int)(off * 8.0f);
        if (b < 30) b = 30;
        r.pixelShade(px * viewScale + viewX, py * viewScale + viewY, b);
    }
}
