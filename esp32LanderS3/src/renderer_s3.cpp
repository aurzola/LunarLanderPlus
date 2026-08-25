#include <Arduino.h>
#include <cmath>
#include <cstring>
#include "renderer_s3.h"

namespace {
const uint8_t WHITE = 255;
}

RendererS3::RendererS3(uint8_t *fb, int w, int h)
    : fb_(fb), w_(w), h_(h)
{
}

void RendererS3::clear()
{
    memset(fb_, 0, (size_t)w_ * (size_t)h_);
}

void RendererS3::pixel(float x, float y)
{
    int px = (int)roundf(x);
    int py = (int)roundf(y);
    if (px < 0 || px >= w_ || py < 0 || py >= h_) return;
    fb_[py * w_ + px] = WHITE;
}

void RendererS3::pixelShade(float x, float y, int brightness)
{
    int px = (int)roundf(x);
    int py = (int)roundf(y);
    if (px < 0 || px >= w_ || py < 0 || py >= h_) return;
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    fb_[py * w_ + px] = (uint8_t)brightness;
}

void RendererS3::flush()
{
}

int RendererS3::width() const { return w_; }
int RendererS3::height() const { return h_; }
