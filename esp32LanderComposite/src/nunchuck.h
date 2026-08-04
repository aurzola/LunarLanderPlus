#pragma once
#include <stdint.h>

#define NUNCHUCK_SDA 21
#define NUNCHUCK_SCL 22
#define NUNCHUCK_ADDR 0x52

class Nunchuck
{
public:
    bool begin();
    bool read();
    int joystickX() const { return jsX; }
    int joystickY() const { return jsY; }
    bool buttonC() const { return btnC; }
    bool buttonZ() const { return btnZ; }

private:
    bool init();
    int jsX = 128;
    int jsY = 128;
    bool btnC = false;
    bool btnZ = false;
};
