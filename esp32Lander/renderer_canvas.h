#ifndef RENDERER_CANVAS_H
#define RENDERER_CANVAS_H

#include "renderer.h"

class RendererCanvas : public Renderer {
public:
    void line(float x0, float y0, float x1, float y1) override;
    void rect(float x, float y, float w, float h) override;
    void circle(float cx, float cy, float r) override;
    void text(float x, float y, const char *s) override;
};

#endif
