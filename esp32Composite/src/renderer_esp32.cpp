#include <Arduino.h>
#include <cmath>
#include "renderer_esp32.h"

namespace {
const char WHITE = 100;
}

RendererESP32::RendererESP32(CompositeGraphics &g)
    : g_(g), w_(g.xres), h_(g.yres)
{
}

void RendererESP32::clear()
{
    g_.begin(0);
}

void RendererESP32::pixel(float x, float y)
{
    g_.dot((int)roundf(x), (int)roundf(y), WHITE);
}

void RendererESP32::flush()
{
    g_.end();
}

int RendererESP32::width() const { return w_; }
int RendererESP32::height() const { return h_; }
