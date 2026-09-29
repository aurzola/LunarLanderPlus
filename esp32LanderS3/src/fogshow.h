#ifndef FOGSHOW_H
#define FOGSHOW_H

#include "renderer.h"
#include "terrain.h"

// TEMP showcase (rama titan-fog-showcase): draws the Titan fog band split into
// four quadrants, each rendered with a different fog style so the look can be
// compared on the CRT and one picked for the real atmosphere. All four use a
// single band (FOG_BAND_COUNT==1 concept) over the same Titan terrain.
namespace FogShow {
    void draw(Renderer &r, Terrain &t, float counter);
}

#endif