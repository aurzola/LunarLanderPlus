#include <Arduino.h>
#include <esp_pm.h>
#include <esp_system.h>
#include <esp_random.h>
#include <sys/time.h>

#include "src/CompositeGraphics.h"
#include "src/CompositeOutput.h"

#include "src/config.h"
#include "src/game.h"
#include "src/renderer_esp32.h"

const int XRES = (int)SCREEN_W;
const int YRES = (int)SCREEN_H;

const int PIN_POT = 34;
const int PIN_TRIGGER = 35;
const int PIN_START = 13;

const float TRIGGER_OFF_VOLT = 1.0f;
const float TRIGGER_MIN_VOLT = 2.5f;
const float TRIGGER_MAX_VOLT = 2.9f;

CompositeGraphics graphics(XRES, YRES);
CompositeOutput composite(CompositeOutput::NTSC, XRES * 2, YRES * 2);
RendererESP32 renderer(graphics);
Game game;

static int smoothPot = 0;
static int smoothTrigger = 0;
static int lastButton = HIGH;

static void compositeCore(void *)
{
    while (true) composite.sendFrameHalfResolution(&graphics.frame);
}

static int lowPass(int prev, int raw)
{
    return (prev * 7 + raw) / 8;
}

static void readInputs()
{
    game.input.startPressed = false;
    smoothPot = lowPass(smoothPot, analogRead(PIN_POT));
    smoothTrigger = lowPass(smoothTrigger, analogRead(PIN_TRIGGER));

    float t = (float)smoothPot / 4095.0f;
    if (t < 0.05f) t = 0.05f;
    else if (t > 0.95f) t = 0.95f;
    float a = (t - 0.05f) / 0.90f;
    game.input.angle = -PI * a;

    float v = smoothTrigger * (3.3f / 4095.0f);
    int throttle;
    if (v < TRIGGER_OFF_VOLT) {
        throttle = 0;
    } else if (v < TRIGGER_MIN_VOLT) {
        throttle = 1;
    } else {
        float q = (v - TRIGGER_MIN_VOLT) / (TRIGGER_MAX_VOLT - TRIGGER_MIN_VOLT);
        if (q < 0.0f) q = 0.0f;
        else if (q > 1.0f) q = 1.0f;
        throttle = 1 + (int)(sqrtf(q) * 7.99f);
    }
    game.input.throttle = throttle;

    int b = digitalRead(PIN_START);
    if (lastButton == HIGH && b == LOW) game.input.startPressed = true;
    lastButton = b;
}

void setup()
{
    esp_pm_lock_handle_t pmLock;
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "compositeCorePerformanceLock", &pmLock);
    esp_pm_lock_acquire(pmLock);

    struct timeval tv;
    tv.tv_sec = (time_t)(esp_random() & 0x7FFFFFFF);
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    pinMode(PIN_POT, INPUT);
    pinMode(PIN_TRIGGER, INPUT);
    pinMode(PIN_START, INPUT_PULLUP);

    composite.init();
    graphics.init();

    xTaskCreatePinnedToCore(compositeCore, "compositeCore", 1024, NULL, 1, NULL, 0);
}

void loop()
{
    const unsigned long stepMillis = (unsigned long)(GAME_DT * 1000.0f);
    static unsigned long acc = 0;
    static unsigned long last = 0;

    readInputs();

    unsigned long now = millis();
    if (last == 0) last = now;
    unsigned long delta = now - last;
    last = now;
    if (delta > 40) delta = 40;
    acc += delta;
    while (acc >= stepMillis) {
        game.update();
        acc -= stepMillis;
    }

    game.draw(renderer);
    renderer.flush();
}
