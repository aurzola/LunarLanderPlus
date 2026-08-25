#ifndef RENDERER_S3_H
#define RENDERER_S3_H

#include <cstdint>
#include "renderer_canvas.h"

class RendererS3 : public RendererCanvas {
public:
    RendererS3(uint8_t *fb, int w, int h);

    void clear() override;
    void pixel(float x, float y) override;
    void pixelShade(float x, float y, int brightness) override;
    void flush() override;
    int width() const override;
    int height() const override;

private:
    uint8_t *fb_;
    int w_;
    int h_;
};

#endif
