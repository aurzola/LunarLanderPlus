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
#include "src/audio.h"
#include "src/nunchuck.h"

const int XRES = (int)SCREEN_W;
const int YRES = (int)SCREEN_H;

const int PIN_POT = 34;
const int PIN_TRIGGER = 35;
const int PIN_START = 13;

const float POT_DEAD_MIN = 0.02f;
const float POT_DEAD_MAX = 0.98f;

#define CONTROLS_WIRED 1
#define NUNCHUCK_TRIGGER_Z 1

static Game game;
static RendererESP32 *renderer = NULL;

static Nunchuck nunchuck;
static int smoothPot = 0;
static float potLevel = 0.0f;
static bool motorOn = false;
static int lastButton = HIGH;
static int lastGameState = -1;
static unsigned long lastIsrPrint = 0;

static int lowPass(int prev, int raw, int shift)
{
    return (prev * ((1 << shift) - 1) + raw) >> shift;
}

static float readPotLevel()
{
    smoothPot = lowPass(smoothPot, analogRead(PIN_POT), 2);
    float t = (float)smoothPot / 4095.0f;
    if (t < POT_DEAD_MIN) t = POT_DEAD_MIN;
    else if (t > POT_DEAD_MAX) t = POT_DEAD_MAX;
    return (t - POT_DEAD_MIN) / (POT_DEAD_MAX - POT_DEAD_MIN);
}

static float readStickAngle()
{
    int jx = nunchuck.joystickX();
    float dx = (jx - 128) / 127.0f;
    if (dx > -0.10f && dx < 0.10f) return 0.0f;
    if (dx < 0.0f) dx = (dx + 0.10f) / 0.90f;
    else dx = (dx - 0.10f) / 0.90f;
    return dx * (PI / 2.0f);
}

#if 0
// Legacy: gatillo reóstato GPIO35 (divisor 120 Ω, 2.5–2.9 V). Sustituido
// por pot (nivel de potencia) + botón del nunchuck (encendido/apagado motor).
const float TRIGGER_OFF_VOLT = 1.0f;
const float TRIGGER_MIN_VOLT = 2.5f;
const float TRIGGER_MAX_VOLT = 2.9f;
static int smoothTrigger = 0;
static float readTriggerThrust()
{
    smoothTrigger = lowPass(smoothTrigger, analogRead(PIN_TRIGGER), 1);
    float v = smoothTrigger * (3.3f / 4095.0f);
    if (v < 0.7f) return 0.0f;
    if (v < 2.2f) return 0.1f + 0.2f * (v - 0.7f) / 1.5f;
    float q = (v - 2.2f) / 0.7f;
    if (q > 1.0f) q = 1.0f;
    return 0.2f + q * 0.8f;
}
#endif

static void readInputs()
{
    game.input.startPressed = false;

#if CONTROLS_WIRED
    nunchuck.read();

    game.input.angle = readStickAngle();
    potLevel = readPotLevel();

#if NUNCHUCK_TRIGGER_Z
    motorOn = nunchuck.buttonZ();
#else
    motorOn = nunchuck.buttonC();
#endif
    game.input.thrust = motorOn ? potLevel : 0.0f;
#else
    game.input.angle = 0.0f;
    game.input.thrust = 0.0f;
#endif

    int b = digitalRead(PIN_START);
    if (lastButton == HIGH && b == LOW) game.input.startPressed = true;
    lastButton = b;
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
    pinMode(PIN_START, INPUT_PULLUP);

    Serial.printf("[nunchuck] %s\n", nunchuck.begin() ? "ok" : "fail");

    video_graphics(NTSC_320x240, FB_FORMAT_GREY_8BPP);
    renderer = new RendererESP32(video_get_frame_buffer_address(), XRES, YRES);

    Audio::begin();
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

    if (game.state != lastGameState) {
        if (game.state == STATE_CRASHED) Audio::playExplosion();
        lastGameState = game.state;
    }
    Audio::setThrust(game.state == STATE_PLAYING ? game.ship.thrustBuild : 0.0f);

    if (millis() - lastIsrPrint > 1000) {
        lastIsrPrint = millis();
        Serial.printf("[audio] isr=%u\n", (unsigned)Audio::debugIsrCount());
    }

    video_wait_frame();
    game.draw(*renderer);
}
