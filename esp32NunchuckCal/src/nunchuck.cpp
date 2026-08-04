#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>
#include "nunchuck.h"

static const uint8_t KEY = 0x17;

bool Nunchuck::begin()
{
    Wire.begin();
    Wire.setClock(50000);
    Wire.setTimeOut(100);
    gpio_set_pull_mode((gpio_num_t)NUNCHUCK_SDA, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)NUNCHUCK_SCL, GPIO_PULLUP_ONLY);
    delay(10);
    if (!init()) return false;

    long rawSum = 0, decSum = 0;
    const int n = 20;
    for (int i = 0; i < n; i++) {
        uint8_t buf[6];
        if (rawRead(buf)) {
            rawSum += buf[0];
            decSum += (uint8_t)((buf[0] ^ KEY) + KEY);
        }
        delay(3);
    }
    encrypted = (abs((int)(decSum / n) - 128) < abs((int)(rawSum / n) - 128));

    for (int i = 0; i < 3; i++) {
        read();
        delay(10);
    }
    return true;
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

bool Nunchuck::rawRead(uint8_t buf[6])
{
    Wire.beginTransmission(NUNCHUCK_ADDR);
    Wire.write(0x00);
    if (Wire.endTransmission() != 0) return false;
    delay(2);
    if (Wire.requestFrom(NUNCHUCK_ADDR, 6) != 6) return false;
    for (int i = 0; i < 6; i++) buf[i] = (uint8_t)Wire.read();
    return true;
}

bool Nunchuck::read()
{
    uint8_t buf[6];
    if (!rawRead(buf)) {
        errCount++;
        init();
        flush();
        return false;
    }
    if (encrypted) {
        for (int i = 0; i < 6; i++) buf[i] = (uint8_t)((buf[i] ^ KEY) + KEY);
    }

    jsX = buf[0];
    jsY = buf[1];
    btnC = !(buf[5] & 0x02);
    btnZ = !(buf[5] & 0x01);
    return true;
}

void Nunchuck::flush()
{
    for (int i = 0; i < 3; i++) {
        delay(3);
        uint8_t buf[6];
        rawRead(buf);
    }
}
