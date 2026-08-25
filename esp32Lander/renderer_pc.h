#ifndef RENDERER_PC_H
#define RENDERER_PC_H

#include <string>
#include <vector>
#include <cstdint>
#include "renderer_canvas.h"

class RendererPC : public RendererCanvas {
public:
    RendererPC(int w, int h, const std::string &outDir);

    void clear() override;
    void pixel(float x, float y) override;
    void pixelShade(float x, float y, int brightness) override;
    void flush() override;
    int width() const override { return w_; }
    int height() const override { return h_; }
    void drawLayer(const uint8_t *layer, int lw, int lh,
                   float offX, float offY, float scale) override;
    const uint8_t *data() const { return fb_.data(); }

private:
    int w_;
    int h_;
    std::vector<uint8_t> fb_;
    std::string outDir_;
    int frame_;
};

#endif
