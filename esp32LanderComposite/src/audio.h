#pragma once

namespace Audio {

void begin();
void debugBeep();
void setThrust(float level);
void setWind(float level);
void playExplosion();
void playBurn();
void playLightning();
uint32_t debugIsrCount();

}
