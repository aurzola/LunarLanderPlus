#include <Arduino.h>
#include <esp_system.h>

extern "C" {
#include "src/video.h"
}
const int XRES = 320;
const int YRES = 240;
const int PIN_LED = 2;

void setup()
{
    Serial.begin(115200);
    delay(50);
    Serial.printf("BOOT reset_reason=%u cpu=%uMHz AQUATICUS LIB TEST\n",
                  (unsigned)esp_reset_reason(), getCpuFrequencyMhz());

    pinMode(PIN_LED, OUTPUT);

    video_graphics(NTSC_320x240, FB_FORMAT_GREY_8BPP);
    Serial.println("VIDEO_INIT_RETURNED");

    char desc[64];
    video_get_mode_description(desc, sizeof(desc));
    Serial.println(desc);
    Serial.println("SETUP_DONE");
}

void loop()
{
    static uint32_t loops = 0;
    static uint32_t lastPrint = 0;

    uint8_t *fb = video_get_frame_buffer_address();

    for (int y = 0; y < YRES; y++)
    {
        for (int x = 0; x < XRES; x++)
        {
            int bar = (x / (XRES / 5)) % 5;
            fb[y * XRES + x] = (y < YRES / 2) ? (15 + bar * 10) : 0;
        }
    }

    video_wait_frame();

    loops++;
    digitalWrite(PIN_LED, (loops % 50) < 25);

    if (millis() - lastPrint >= 2000)
    {
        lastPrint = millis();
        Serial.printf("alive loops=%lu ms=%lu\n", loops, lastPrint);
    }
}
