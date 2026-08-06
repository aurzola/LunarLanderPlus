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
static unsigned long lastNunchuckPrint = 0;

static const float POWER_STEPS[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
static int powerStep = -1;
static int potAtCycle = 0;
static bool lastC = false;
static float powerLevel = 0.0f;

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

static int stickCenterX = 128;
static int stickLeftDev = 80;
static int stickRightDev = 80;
static int stickObsL = 80;
static int stickObsR = 80;

static void calibrateStick()
{
    long sum = 0;
    const int n = 40;
    for (int i = 0; i < n; i++) {
        nunchuck.read();
        sum += nunchuck.joystickX();
        delay(3);
    }
    stickCenterX = (int)(sum / n);
}

static float readStickAngle()
{
    int jx = nunchuck.joystickX();
    int dl = stickCenterX - jx;
    int dr = jx - stickCenterX;
    if (dl > stickObsL) stickObsL = dl;
    if (dr > stickObsR) stickObsR = dr;

    if (dl > 0) {
        if (dl > stickLeftDev) stickLeftDev = dl;
        else stickLeftDev = (stickLeftDev * 63 + stickObsL) / 64;
        if (stickLeftDev < 40) stickLeftDev = 40;
        if (dl < 10) return 0.0f;
        return -(dl / (float)stickLeftDev) * (PI / 2.0f);
    }
    if (dr > 0) {
        if (dr > stickRightDev) stickRightDev = dr;
        else stickRightDev = (stickRightDev * 63 + stickObsR) / 64;
        if (stickRightDev < 40) stickRightDev = 40;
        if (dr < 10) return 0.0f;
        return (dr / (float)stickRightDev) * (PI / 2.0f);
    }
    return 0.0f;
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

    bool cNow = nunchuck.buttonC();
    if (cNow && !lastC) {
        powerStep = (powerStep + 1) % 5;
        potAtCycle = smoothPot;
    }
    lastC = cNow;

    if (powerStep >= 0) {
        if (abs(smoothPot - potAtCycle) > 120) {
            powerStep = -1;
        } else {
            powerLevel = POWER_STEPS[powerStep];
        }
    }
    if (powerStep < 0) powerLevel = potLevel;

#if NUNCHUCK_TRIGGER_Z
    motorOn = nunchuck.buttonZ();
#else
    motorOn = nunchuck.buttonC();
#endif
    game.input.thrust = motorOn ? powerLevel : 0.0f;
    game.input.powerLevel = powerLevel;
#else
    game.input.angle = 0.0f;
    game.input.thrust = 0.0f;
    game.input.powerLevel = 0.0f;
#endif

    int b = digitalRead(PIN_START);
    if (lastButton == HIGH && b == LOW) game.input.startPressed = true;
    lastButton = b;
}

void setup()
{
    Serial.begin(115200);
    srand(esp_random());

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
    calibrateStick();
    Serial.printf("[nunchuck] center=%d\n", stickCenterX);

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
    Audio::setWind(game.windEnabled && game.state == STATE_PLAYING
                   ? game.windStrength : 0.0f);
    if (game.storm.takeNewBolt()) Audio::playLightning();

    if (millis() - lastIsrPrint > 1000) {
        lastIsrPrint = millis();
        Serial.printf("[audio] isr=%u\n", (unsigned)Audio::debugIsrCount());
    }

    if (millis() - lastNunchuckPrint > 500) {
        lastNunchuckPrint = millis();
        Serial.printf("[nunchuck] x=%d y=%d c=%d z=%d err=%u pwr=%d step=%d\n",
                      nunchuck.joystickX(), nunchuck.joystickY(),
                      nunchuck.buttonC(), nunchuck.buttonZ(),
                      nunchuck.readErrors(), (int)(powerLevel * 100), powerStep);
    }

    video_wait_frame();
    game.draw(*renderer);
}
