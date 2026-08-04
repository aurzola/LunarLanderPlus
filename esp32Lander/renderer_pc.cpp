#include "renderer_pc.h"

#include <cmath>
#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>

RendererPC::RendererPC(int w, int h, const std::string &outDir)
    : w_(w), h_(h), fb_((size_t)w * h, 0), outDir_(outDir), frame_(0)
{
}

void RendererPC::clear()
{
    for (size_t i = 0; i < fb_.size(); i++) fb_[i] = 0;
}

void RendererPC::pixel(float x, float y)
{
    int px = (int)roundf(x);
    int py = (int)roundf(y);
    if (px < 0 || px >= w_ || py < 0 || py >= h_) return;
    fb_[py * w_ + px] = 255;
}

void RendererPC::pixelShade(float x, float y, int brightness)
{
    int px = (int)roundf(x);
    int py = (int)roundf(y);
    if (px < 0 || px >= w_ || py < 0 || py >= h_) return;
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    fb_[py * w_ + px] = (uint8_t)brightness;
}

void RendererPC::flush()
{
    if (outDir_.empty()) return;
    mkdir(outDir_.c_str(), 0755);
    char path[256];
    snprintf(path, sizeof path, "%s/frame_%04d.ppm", outDir_.c_str(), frame_);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", w_, h_);
    for (int i = 0; i < w_ * h_; i++) {
        unsigned char v = fb_[i];
        fputc(v, f);
        fputc(v, f);
        fputc(v, f);
    }
    fclose(f);
    frame_++;
}
