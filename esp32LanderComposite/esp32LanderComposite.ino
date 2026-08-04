#include <Arduino.h>
#include <esp_pm.h>
#include <esp_system.h>
#include <esp_random.h>
#include <sys/time.h>

extern "C" {
#include "src/video.h"
}

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

#define CONTROLS_WIRED 1
#define AUTOSTART_DELAY_MS 4000

static Game game;
static RendererESP32 *renderer = NULL;

static int smoothPot = 0;
static int smoothTrigger = 0;
static int lastButton = HIGH;
static unsigned long menuTimer = 0;

static int lowPass(int prev, int raw, int shift)
{
    return (prev * ((1 << shift) - 1) + raw) >> shift;
}

static void readInputs()
{
    game.input.startPressed = false;

#if CONTROLS_WIRED
    smoothPot = lowPass(smoothPot, analogRead(PIN_POT), 2);
    smoothTrigger = lowPass(smoothTrigger, analogRead(PIN_TRIGGER), 1);

    float t = (float)smoothPot / 4095.0f;
    if (t < 0.02f) t = 0.02f;
    else if (t > 0.98f) t = 0.98f;
    float a = (t - 0.02f) / 0.96f;
    game.input.angle = -PI / 2.0f + PI * a;

    float v = smoothTrigger * (3.3f / 4095.0f);
    float thrust;
    if (v < 0.7f) {
        thrust = 0.0f;
    } else if (v < 2.2f) {
        thrust = 0.1f + 0.2f * (v - 0.7f) / 1.5f;
    } else {
        float q = (v - 2.2f) / 0.7f;
        if (q > 1.0f) q = 1.0f;
        thrust = 0.2f + q * 0.8f;
    }
    game.input.thrust = thrust;
#else
    game.input.angle = 0.0f;
    game.input.thrust = 0;
#endif

    int b = digitalRead(PIN_START);
    if (lastButton == HIGH && b == LOW) game.input.startPressed = true;
    lastButton = b;
}

static void autostart()
{
    if (game.state == STATE_WAITING) {
        if (menuTimer == 0) menuTimer = millis();
        else if (millis() - menuTimer > AUTOSTART_DELAY_MS) {
            game.input.startPressed = true;
            menuTimer = 0;
        }
    } else {
        menuTimer = 0;
    }
}

void setup()
{
    Serial.begin(115200);

    esp_pm_lock_handle_t pmLock;
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "gamePerfLock", &pmLock);
    esp_pm_lock_acquire(pmLock);

    struct timeval tv;
    tv.tv_sec = (time_t)(esp_random() & 0x7FFFFFFF);
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    pinMode(PIN_POT, INPUT);
    pinMode(PIN_TRIGGER, INPUT);
    pinMode(PIN_START, INPUT_PULLUP);

    video_graphics(NTSC_320x240, FB_FORMAT_GREY_8BPP);
    renderer = new RendererESP32(video_get_frame_buffer_address(), XRES, YRES);
}

void loop()
{
    const unsigned long stepMillis = (unsigned long)(GAME_DT * 1000.0f);
    static unsigned long acc = 0;
    static unsigned long last = 0;
    readInputs();
    autostart();

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

    video_wait_frame();
    game.draw(*renderer);
}
