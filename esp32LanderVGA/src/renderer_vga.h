#ifndef RENDERER_VGA_H
#define RENDERER_VGA_H

#include <cstdint>
#include "renderer_canvas.h"

class RendererVGA : public RendererCanvas {
public:
    RendererVGA(uint8_t *back, uint8_t *front, int w, int h,
                void (*waitVBlank)());

    void clear() override;
    void pixel(float x, float y) override;
    void pixelShade(float x, float y, int brightness) override;
    void flush() override;
    int width() const override;
    int height() const override;

private:
    uint8_t *back_;
    uint8_t *front_;
    int w_;
    int h_;
    void (*waitVBlank_)();
};

#endif
