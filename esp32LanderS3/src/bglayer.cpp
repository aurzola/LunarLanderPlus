#include <cstdlib>
#include <cstring>
#include <cmath>
#include "bglayer.h"

static void *(*bgAllocFn)(size_t) = malloc;
static void (*bgFreeFn)(void *) = free;

void bgSetAllocator(void *(*fn)(size_t))
{
    if (fn) {
        bgAllocFn = fn;
        bgFreeFn = free;
    }
}

uint8_t *bgAllocBuffer(size_t bytes)
{
    return (uint8_t *)bgAllocFn(bytes);
}

void bgFreeBuffer(uint8_t *p)
{
    if (p) bgFreeFn(p);
}

void LayerPainter::begin(uint8_t *buf, int w, int h)
{
    buf_ = buf;
    w_ = w;
    h_ = h;
}

void LayerPainter::clear()
{
    memset(buf_, 0, (size_t)w_ * (size_t)h_);
}

void LayerPainter::pixel(float x, float y)
{
    int px = (int)roundf(x), py = (int)roundf(y);
    if (px < 0 || px >= w_ || py < 0 || py >= h_) return;
    buf_[(size_t)py * w_ + px] = 255;
}

void LayerPainter::pixelShade(float x, float y, int brightness)
{
    int px = (int)roundf(x), py = (int)roundf(y);
    if (px < 0 || px >= w_ || py < 0 || py >= h_) return;
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    buf_[(size_t)py * w_ + px] = (uint8_t)brightness;
}

void LayerPainter::vspan(int x, int y0, int y1)
{
    if (x < 0 || x >= w_) return;
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (y0 < 0) y0 = 0;
    if (y1 >= h_) y1 = h_ - 1;
    if (y0 > y1) return;
    memset(buf_ + (size_t)y0 * w_ + x, 255, (size_t)(y1 - y0 + 1));
}

void LayerPainter::line(float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1, dy = y2 - y1;
    if (fabsf(dx) < 0.001f) {
        vspan((int)roundf(x1), (int)roundf(y1), (int)roundf(y2));
        return;
    }
    float slope = dy / dx;
    int ix0 = (int)floorf(fminf(x1, x2));
    int ix1 = (int)ceilf(fmaxf(x1, x2));
    int prevY = (int)roundf(y1 + ((float)ix0 - x1) * slope);
    for (int x = ix0; x <= ix1; x++) {
        int y = (int)roundf(y1 + ((float)x - x1) * slope);
        vspan(x, prevY, y);
        prevY = y;
    }
}

bool BgLayer::alloc(int width_, int height_)
{
    release();
    buf = bgAllocBuffer((size_t)width_ * (size_t)height_);
    if (!buf) return false;
    w = width_;
    h = height_;
    memset(buf, 0, (size_t)w * (size_t)h);
    return true;
}

void BgLayer::release()
{
    if (buf) bgFreeBuffer(buf);
    buf = 0;
    w = 0;
    h = 0;
}
