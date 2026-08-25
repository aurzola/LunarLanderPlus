#ifndef RENDERER_H
#define RENDERER_H

#include <cstdint>

class Renderer {
public:
    virtual ~Renderer() {}
    virtual void clear() = 0;
    virtual void pixel(float x, float y) = 0;
    virtual void pixelShade(float x, float y, int brightness) = 0;
    virtual void line(float x0, float y0, float x1, float y1) = 0;
    virtual void lineShade(float x0, float y0, float x1, float y1, int brightness) = 0;
    virtual void rect(float x, float y, float w, float h) = 0;
    virtual void circle(float cx, float cy, float r) = 0;
    virtual void text(float x, float y, const char *s) = 0;
    virtual void textScaled(float x, float y, const char *s, float scale, int brightness) = 0;
    virtual void rectShade(float x, float y, float w, float h, int brightness) = 0;
    virtual void fillPolygon(const float* xs, const float* ys, int n, int brightness) = 0;
    virtual void setClip(float x, float y, float w, float h) = 0;
    virtual void clearClip() = 0;
    virtual void flush() = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;

    virtual void drawLayer(const uint8_t *layer, int lw, int lh,
                           float offX, float offY, float scale)
    {
        (void)layer; (void)lw; (void)lh;
        (void)offX; (void)offY; (void)scale;
    }
};

#endif
