#ifndef RENDERER_ESP32_H
#define RENDERER_ESP32_H

#include <cstdint>
#include "renderer_canvas.h"

class RendererESP32 : public RendererCanvas {
public:
    RendererESP32(uint8_t *fb, int w, int h);

    void clear() override;
    void pixel(float x, float y) override;
    void flush() override;
    int width() const override;
    int height() const override;

private:
    uint8_t *fb_;
    int w_;
    int h_;
};

#endif
