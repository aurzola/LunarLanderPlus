#include <Arduino.h>
#include <cmath>
#include <cstring>
#include "renderer_esp32.h"

namespace {
const uint8_t WHITE = 255;
}

RendererESP32::RendererESP32(uint8_t *fb, int w, int h)
    : fb_(fb), w_(w), h_(h)
{
}

void RendererESP32::clear()
{
    memset(fb_, 0, (size_t)w_ * (size_t)h_);
}

void RendererESP32::pixel(float x, float y)
{
    int px = (int)roundf(x);
    int py = (int)roundf(y);
    if (px < 0 || px >= w_ || py < 0 || py >= h_) return;
    fb_[py * w_ + px] = WHITE;
}

void RendererESP32::pixelShade(float x, float y, int brightness)
{
    int px = (int)roundf(x);
    int py = (int)roundf(y);
    if (px < 0 || px >= w_ || py < 0 || py >= h_) return;
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    fb_[py * w_ + px] = (uint8_t)brightness;
}

void RendererESP32::flush()
{
}

int RendererESP32::width() const { return w_; }
int RendererESP32::height() const { return h_; }
