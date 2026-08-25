#ifndef BGLAYER_H
#define BGLAYER_H

#include <cstddef>
#include <cstdint>
#include "renderer_canvas.h"

void bgSetAllocator(void *(*fn)(size_t));

class LayerPainter : public RendererCanvas {
public:
    void begin(uint8_t *buf, int w, int h);
    void clear() override;
    void pixel(float x, float y) override;
    void pixelShade(float x, float y, int brightness) override;
    void line(float x1, float y1, float x2, float y2) override;
    void flush() override {}
    int width() const override { return w_; }
    int height() const override { return h_; }

private:
    void vspan(int x, int y0, int y1);
    uint8_t *buf_;
    int w_;
    int h_;
};

class BgLayer {
public:
    BgLayer() : buf(0), w(0), h(0) {}
    ~BgLayer() { release(); }
    bool alloc(int w, int h);
    void release();
    bool ready() const { return buf != 0; }
    uint8_t *data() { return buf; }
    const uint8_t *data() const { return buf; }
    int width() const { return w; }
    int height() const { return h; }

private:
    uint8_t *buf;
    int w;
    int h;
};

#endif
