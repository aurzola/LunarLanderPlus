#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>
#include "nunchuck.h"

static const uint8_t KEY = 0x17;

bool Nunchuck::begin()
{
    Wire.begin();
    Wire.setClock(100000);
    gpio_set_pull_mode((gpio_num_t)NUNCHUCK_SDA, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)NUNCHUCK_SCL, GPIO_PULLUP_ONLY);
    delay(10);
    return init();
}

bool Nunchuck::init()
{
    Wire.beginTransmission(NUNCHUCK_ADDR);
    Wire.write(0xF0);
    Wire.write(0x55);
    if (Wire.endTransmission() != 0) return false;

    Wire.beginTransmission(NUNCHUCK_ADDR);
    Wire.write(0xFB);
    Wire.write(0x00);
    return Wire.endTransmission() == 0;
}

bool Nunchuck::read()
{
    Wire.beginTransmission(NUNCHUCK_ADDR);
    Wire.write(0x00);
    if (Wire.endTransmission() != 0) {
        init();
        return false;
    }
    if (Wire.requestFrom(NUNCHUCK_ADDR, 6) != 6) {
        init();
        return false;
    }

    uint8_t buf[6];
    for (int i = 0; i < 6; i++) buf[i] = (uint8_t)Wire.read();
    for (int i = 0; i < 6; i++) buf[i] = (uint8_t)((buf[i] ^ KEY) + KEY);

    jsX = buf[0];
    jsY = buf[1];
    btnC = !(buf[5] & 0x02);
    btnZ = !(buf[5] & 0x01);
    return true;
}
