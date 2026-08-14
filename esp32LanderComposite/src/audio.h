#pragma once

namespace Audio {

void begin();
void setThrust(float level);
void setWind(float level);
void playExplosion();
void playTankerExplosion();
void playBurn();
void playLightning();
void playQuake();
uint32_t debugIsrCount();

}
