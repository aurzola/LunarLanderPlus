#ifndef RENDERER_H
#define RENDERER_H

class Renderer {
public:
    virtual ~Renderer() {}
    virtual void clear() = 0;
    virtual void pixel(float x, float y) = 0;
    virtual void pixelShade(float x, float y, int brightness) = 0;
    virtual void line(float x0, float y0, float x1, float y1) = 0;
    virtual void rect(float x, float y, float w, float h) = 0;
    virtual void circle(float cx, float cy, float r) = 0;
    virtual void text(float x, float y, const char *s) = 0;
    virtual void textScaled(float x, float y, const char *s, float scale, int brightness) = 0;
    virtual void flush() = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
};

#endif
