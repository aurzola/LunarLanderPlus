#ifndef RENDERER_ESP32_H
#define RENDERER_ESP32_H

#include "renderer_canvas.h"
#include "CompositeGraphics.h"

class RendererESP32 : public RendererCanvas {
public:
    RendererESP32(CompositeGraphics &g);

    void clear() override;
    void pixel(float x, float y) override;
    void flush() override;
    int width() const override;
    int height() const override;

private:
    CompositeGraphics &g_;
    int w_;
    int h_;
};

#endif
