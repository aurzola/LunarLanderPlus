#include <Arduino.h>
#include <esp_pm.h>
#include <esp_system.h>
#include <esp_random.h>
#include <sys/time.h>
#include <cstdio>

extern "C" {
#include "src/video.h"
}

#include "src/renderer_esp32.h"
#include "src/nunchuck.h"

const int XRES = 320;
const int YRES = 240;

static RendererESP32 *renderer = NULL;
static Nunchuck nunchuck;

static int centerX = 128;
static int centerY = 128;
static int minX = 255, maxX = 0, minY = 255, maxY = 0;

static void calibrate()
{
    long sx = 0, sy = 0;
    const int n = 40;
    for (int i = 0; i < n; i++) {
        nunchuck.read();
        sx += nunchuck.joystickX();
        sy += nunchuck.joystickY();
        delay(3);
    }
    centerX = (int)(sx / n);
    centerY = (int)(sy / n);
}

void setup()
{
    Serial.begin(115200);

    esp_pm_lock_handle_t pmLock;
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "calPerfLock", &pmLock);
    esp_pm_lock_acquire(pmLock);

    struct timeval tv;
    tv.tv_sec = (time_t)(esp_random() & 0x7FFFFFFF);
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    Serial.printf("[cal] nunchuck %s\n", nunchuck.begin() ? "ok" : "fail");
    if (!nunchuck.read()) {
        Serial.println("[cal] warning: read failed at boot");
    }

    video_graphics(NTSC_320x240, FB_FORMAT_GREY_8BPP);
    renderer = new RendererESP32(video_get_frame_buffer_address(), XRES, YRES);

    calibrate();
    Serial.printf("[cal] center=%d,%d\n", centerX, centerY);
    Serial.println("[cal] sweep stick to all extremes");
}

void loop()
{
    nunchuck.read();
    int x = nunchuck.joystickX();
    int y = nunchuck.joystickY();
    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;

    if (nunchuck.buttonC() && nunchuck.buttonZ()) {
        minX = 255; maxX = 0; minY = 255; maxY = 0;
    }

    renderer->clear();

    renderer->text(72, 12, "NUNCHUCK CALIBRATE");
    renderer->text(20, 30, "MOVE STICK TO EXTREMES");
    renderer->text(20, 40, "C+Z RESETS MIN/MAX");

    char buf[40];
    snprintf(buf, sizeof buf, "X  %3d", x);
    renderer->text(30, 66, buf);
    snprintf(buf, sizeof buf, "Y  %3d", y);
    renderer->text(30, 78, buf);
    snprintf(buf, sizeof buf, "C %d   Z %d   ERR %lu", nunchuck.buttonC(), nunchuck.buttonZ(), nunchuck.readErrors());
    renderer->text(30, 96, buf);
    snprintf(buf, sizeof buf, "ENCRYPTED %d", nunchuck.isEncrypted());
    renderer->text(30, 108, buf);

    snprintf(buf, sizeof buf, "Xmin %3d   Xmax %3d", minX, maxX);
    renderer->text(30, 126, buf);
    snprintf(buf, sizeof buf, "Ymin %3d   Ymax %3d", minY, maxY);
    renderer->text(30, 138, buf);
    snprintf(buf, sizeof buf, "CENTER %3d, %3d", centerX, centerY);
    renderer->text(30, 156, buf);

    renderer->flush();
    video_wait_frame();
}
