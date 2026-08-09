#ifndef RENDERER_CANVAS_H
#define RENDERER_CANVAS_H

#include "renderer.h"

class RendererCanvas : public Renderer {
public:
    RendererCanvas() : clipOn(false), clipX(0), clipY(0), clipW(0), clipH(0) {}
    void line(float x0, float y0, float x1, float y1) override;
    void lineShade(float x0, float y0, float x1, float y1, int brightness) override;
    void rect(float x, float y, float w, float h) override;
    void circle(float cx, float cy, float r) override;
    void text(float x, float y, const char *s) override;
    void textScaled(float x, float y, const char *s, float scale, int brightness) override;
    void setClip(float x, float y, float w, float h) override;
    void clearClip() override;

protected:
    bool clipTest(float x, float y) const;
    void px(float x, float y);
    void pxShade(float x, float y, int b);

private:
    bool clipOn;
    float clipX, clipY, clipW, clipH;
};

#endif
