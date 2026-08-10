#include <Arduino.h>
#include <cmath>
#include <cstring>
#include "renderer_vga.h"

namespace {
const uint8_t WHITE = 200;

// 8x8 Bayer ordered dither matrix, normalized to 0–63.
const uint8_t BAYER[8][8] = {
    {  0, 32,  8, 40,  2, 34, 10, 42 },
    { 48, 16, 56, 24, 50, 18, 58, 26 },
    { 12, 44,  4, 36, 14, 46,  6, 38 },
    { 60, 28, 52, 20, 62, 30, 54, 22 },
    {  3, 35, 11, 43,  1, 33,  9, 41 },
    { 51, 19, 59, 27, 49, 17, 57, 25 },
    { 15, 47,  7, 39, 13, 45,  5, 37 },
    { 63, 31, 55, 23, 61, 29, 53, 21 }
};

// Precomputed thresholds: BAYER[y][x] * 4 + 2 → range 2–254.
const uint8_t TH[8][8] = {
    {  2,130, 34,162, 10,138, 42,170 },
    {194, 66,226, 98,202, 74,234,106 },
    { 50,178, 18,146, 58,186, 26,154 },
    {242,114,210, 82,250,122,218, 90 },
    { 14,142, 46,174,  6,134, 38,166 },
    {206, 78,238,110,198, 70,230,102 },
    { 62,190, 30,158, 54,182, 22,150 },
    {254,126,222, 94,246,118,214, 86 }
};
}

RendererVGA::RendererVGA(uint8_t *back, uint8_t *front, int w, int h,
                         void (*waitVBlank)())
    : back_(back), front_(front), w_(w), h_(h), waitVBlank_(waitVBlank)
{
}

void RendererVGA::clear()
{
    memset(back_, 0, (size_t)w_ * (size_t)h_);
}

void RendererVGA::pixel(float x, float y)
{
    int px = (int)roundf(x);
    int py = (int)roundf(y);
    if (px < 0 || px >= w_ || py < 0 || py >= h_) return;
    back_[py * w_ + px] = WHITE;
}

void RendererVGA::pixelShade(float x, float y, int brightness)
{
    int px = (int)roundf(x);
    int py = (int)roundf(y);
    if (px < 0 || px >= w_ || py < 0 || py >= h_) return;
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    back_[py * w_ + px] = (uint8_t)brightness;
}

void RendererVGA::flush()
{
    // Bayer ordered dithering in-place on back_ (done before the vertical
    // blanking window so the per-pixel loop doesn't race the ISR).
    // Values 0 and 255 pass through unchanged; everything else is compared
    // against a precomputed 8x8 threshold to produce a binary dither pattern.
    // The memcpy inside vblank stays fast because back_ already holds the
    // final values.
    for (int y = 0; y < h_; y++) {
        int by = y & 7;
        for (int x = 0; x < w_; x++) {
            uint8_t v = back_[y * w_ + x];
            if (v != 0 && v != 255) {
                if (v <= TH[by][x & 7]) v = 0;
                else v = 255;
            }
            back_[y * w_ + x] = v;
        }
    }

    waitVBlank_();
    memcpy(front_, back_, (size_t)w_ * (size_t)h_);
}

int RendererVGA::width() const { return w_; }
int RendererVGA::height() const { return h_; }
