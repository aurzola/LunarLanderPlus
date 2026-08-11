#pragma once

namespace Audio {

void begin();
void setThrust(float level);
void setWind(float level);
void playExplosion();
void playTankerExplosion();
void playBurn();
void playLightning();
uint32_t debugIsrCount();

}
