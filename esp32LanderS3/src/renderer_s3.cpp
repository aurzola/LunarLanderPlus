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

void RendererS3::drawLayer(const uint8_t *layer, int lw, int lh,
                           float offX, float offY, float scale)
{
    if (!layer || scale <= 0.0f) return;
    const float inv = 1.0f / scale;
    const int32_t step16 = (int32_t)(inv * 65536.0f);
    const int32_t x0_16 = (int32_t)((-offX * inv) * 65536.0f);

    int wxMin = (int)floorf(-offX * inv);
    int wxMax = (int)ceilf(((float)w_ - offX) * inv);

    int sxStart = (int)(wxMin * scale + offX);
    int sxEnd = (int)(wxMax * scale + offX);
    if (sxStart < 0) sxStart = 0;
    if (sxEnd > w_) sxEnd = w_;

    if (lw <= 0) return;

    for (int sy = 0; sy < h_; sy++) {
        int ly = (int)((sy - offY) * inv);
        if (ly < 0 || ly >= lh) continue;
        const uint8_t *lrow = layer + (size_t)ly * lw;
        uint8_t *frow = fb_ + (size_t)sy * w_;
        int32_t wx16 = x0_16 + (int32_t)((float)sxStart * inv * 65536.0f);
        for (int sx = sxStart; sx < sxEnd; sx++, wx16 += step16) {
            // Wrap horizontally: the baked strip is one tile of a circular
            // world, so screen X beyond it samples the neighbouring copy
            // (instead of clamping, which painted a solid vertical stripe of
            // the edge column at the left/right screen edges).
            int idx = wx16 >> 16;
            if (idx >= 0 && idx < lw) {
                frow[sx] = lrow[idx];
            } else {
                idx %= lw;
                if (idx < 0) idx += lw;
                frow[sx] = lrow[idx];
            }
        }
    }
}

void RendererS3::flush()
{
}

int RendererS3::width() const { return w_; }
int RendererS3::height() const { return h_; }
