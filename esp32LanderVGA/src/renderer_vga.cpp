#include <Arduino.h>
#include <cmath>
#include <cstring>
#include "renderer_vga.h"

namespace {
const uint8_t WHITE = 255;
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
    // The line ISR reads front_; paint happens in back_. Swap in the vertical
    // blanking window so the DMA/ISR never scans a half-updated frame.
    waitVBlank_();
    memcpy(front_, back_, (size_t)w_ * (size_t)h_);
}

int RendererVGA::width() const { return w_; }
int RendererVGA::height() const { return h_; }
