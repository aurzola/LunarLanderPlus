#include <Arduino.h>
#include <Preferences.h>
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

#define CONTROLS_WIRED 1
#define NUNCHUCK_TRIGGER_Z 1
#define POT_DISABLED 1 // pot del PWR deshabilitado (ADC ruidoso reseteaba el nivel);
                       // la potencia se fija solo con C + stick

#if !POT_DISABLED
const float POT_DEAD_MIN = 0.02f;
const float POT_DEAD_MAX = 0.98f;
#endif

static Game game;
static RendererESP32 *renderer = NULL;

// Double buffer: game.draw() paints into fbShadow; right after
// video_wait_frame() (visible field ended, blanking window) the completed
// frame is copied to the DMA framebuffer. The video DMA therefore only ever
// scans complete frames — no tearing/flicker even when a frame is expensive
// to draw (zoom, twister, atmosphere).
static uint8_t fbShadow[XRES * YRES];

static Nunchuck nunchuck;
#if !POT_DISABLED
static int smoothPot = 0;
static float potLevel = 0.0f;
static int potAtCycle = 0;
#endif
static bool motorOn = false;
static int lastButton = HIGH;
static int lastGameState = -1;
static unsigned long lastIsrPrint = 0;
static unsigned long lastNunchuckPrint = 0;

static const float PWR_STICK_RATE = 0.008f;
static bool pwrStickActive = false;
static float powerLevel = 0.5f;

#if !POT_DISABLED
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
#endif

static int stickCenterX = 128;
static int stickLeftDev = 80;
static int stickRightDev = 80;
static int stickObsL = 80;
static int stickObsR = 80;
static int stickCenterY = 128;
static int stickUpDev = 80;
static int stickDownDev = 80;
static int stickObsU = 80;
static int stickObsD = 80;

// Joystick calibration mode (visual, entered by holding C+Z at the title).
static bool calibrateMode = false;
static bool useFixedCal = false;   // true once a manual calibration is saved/loaded
static int calMinX = 255, calMaxX = 0;
static int calMinY = 255, calMaxY = 0;
static unsigned long calEnterAt = 0; // debounce: require C+Z held 0.5 s
static const unsigned long CAL_HOLD_MS = 500;

static void calibrateStick()
{
    long sx = 0, sy = 0;
    const int n = 40;
    for (int i = 0; i < n; i++) {
        nunchuck.read();
        sx += nunchuck.joystickX();
        sy += nunchuck.joystickY();
        delay(3);
    }
    stickCenterX = (int)(sx / n);
    stickCenterY = (int)(sy / n);
}

static void saveCalibration()
{
    Preferences p;
    p.begin("jsCal", false);
    p.putBool("set", true);
    p.putInt("cx", stickCenterX);
    p.putInt("cy", stickCenterY);
    p.putInt("ld", stickLeftDev);
    p.putInt("rd", stickRightDev);
    p.putInt("ud", stickUpDev);
    p.putInt("dd", stickDownDev);
    p.end();
}

static bool loadCalibration()
{
    Preferences p;
    p.begin("jsCal", true);
    bool set = p.getBool("set", false);
    if (set) {
        stickCenterX = p.getInt("cx", 128);
        stickCenterY = p.getInt("cy", 128);
        stickLeftDev = p.getInt("ld", 80);
        stickRightDev = p.getInt("rd", 80);
        stickUpDev = p.getInt("ud", 80);
        stickDownDev = p.getInt("dd", 80);
    }
    p.end();
    return set;
}

// Capture the live stick range during a calibration session and, on confirm,
// derive a fixed center + per-axis max deviation that replace the adaptive
// boot calibration (which drifts if the stick wasn't centred at power-up).
static void beginCalibration()
{
    calibrateMode = true;
    calMinX = 255; calMaxX = 0;
    calMinY = 255; calMaxY = 0;
}

static void confirmCalibration()
{
    if (calMinX <= calMaxX) {
        stickCenterX = (calMinX + calMaxX) / 2;
        stickLeftDev = stickCenterX - calMinX;
        stickRightDev = calMaxX - stickCenterX;
    }
    if (calMinY <= calMaxY) {
        stickCenterY = (calMinY + calMaxY) / 2;
        stickUpDev = calMaxY - stickCenterY;
        stickDownDev = stickCenterY - calMinY;
    }
    if (stickLeftDev < 40) stickLeftDev = 40;
    if (stickRightDev < 40) stickRightDev = 40;
    if (stickUpDev < 40) stickUpDev = 40;
    if (stickDownDev < 40) stickDownDev = 40;
    useFixedCal = true;
    saveCalibration();
    calibrateMode = false;
}

static void cancelCalibration()
{
    calibrateMode = false;
}

static float readStickAngle()
{
    int jx = nunchuck.joystickX();
    int dl = stickCenterX - jx;
    int dr = jx - stickCenterX;
    if (!useFixedCal) {
        if (dl > stickObsL) stickObsL = dl;
        if (dr > stickObsR) stickObsR = dr;
    }

    if (dl > 0) {
        if (!useFixedCal) {
            if (dl > stickLeftDev) stickLeftDev = dl;
            else stickLeftDev = (stickLeftDev * 63 + stickObsL) / 64;
            if (stickLeftDev < 40) stickLeftDev = 40;
        }
        if (dl < 10) return 0.0f;
        return -(dl / (float)stickLeftDev) * (PI / 2.0f);
    }
    if (dr > 0) {
        if (!useFixedCal) {
            if (dr > stickRightDev) stickRightDev = dr;
            else stickRightDev = (stickRightDev * 63 + stickObsR) / 64;
            if (stickRightDev < 40) stickRightDev = 40;
        }
        if (dr < 10) return 0.0f;
        return (dr / (float)stickRightDev) * (PI / 2.0f);
    }
    return 0.0f;
}

static float readStickYDev()
{
    int jy = nunchuck.joystickY();
    int du = jy - stickCenterY;
    int dd = stickCenterY - jy;
    if (!useFixedCal) {
        if (du > stickObsU) stickObsU = du;
        if (dd > stickObsD) stickObsD = dd;
    }

    if (du > 0) {
        if (!useFixedCal) {
            if (du > stickUpDev) stickUpDev = du;
            else stickUpDev = (stickUpDev * 63 + stickObsU) / 64;
            if (stickUpDev < 40) stickUpDev = 40;
        }
        if (du < 10) return 0.0f;
        return (du / (float)stickUpDev);
    }
    if (dd > 0) {
        if (!useFixedCal) {
            if (dd > stickDownDev) stickDownDev = dd;
            else stickDownDev = (stickDownDev * 63 + stickObsD) / 64;
            if (stickDownDev < 40) stickDownDev = 40;
        }
        if (dd < 10) return 0.0f;
        return -(dd / (float)stickDownDev);
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

static void drawCalibration(Renderer &r);

static void readInputs()
{
    game.input.startPressed = false;

#if CONTROLS_WIRED
    nunchuck.read();

    // Joystick calibration mode (only reachable from the title screen).
    if (calibrateMode) {
        int jx = nunchuck.joystickX();
        int jy = nunchuck.joystickY();
        if (jx < calMinX) calMinX = jx;
        if (jx > calMaxX) calMaxX = jx;
        if (jy < calMinY) calMinY = jy;
        if (jy > calMaxY) calMaxY = jy;
        int b = digitalRead(PIN_START);
        if (lastButton == HIGH && b == LOW) { cancelCalibration(); lastButton = b; return; }
        lastButton = b;
        if (nunchuck.buttonC() && !nunchuck.buttonZ()) confirmCalibration();
        return;
    }

    // Hold C+Z together at the title to enter joystick calibration.
    if (game.state == STATE_WAITING) {
        if (nunchuck.buttonC() && nunchuck.buttonZ()) {
            if (calEnterAt == 0) calEnterAt = millis();
            else if (millis() - calEnterAt >= CAL_HOLD_MS) { beginCalibration(); return; }
        } else {
            calEnterAt = 0;
        }
    }

    game.input.angle = readStickAngle();
#if !POT_DISABLED
    potLevel = readPotLevel();
#endif

    bool cNow = nunchuck.buttonC();
    float yDev = readStickYDev();
    if (cNow && (yDev > 0.1f || yDev < -0.1f)) {
        if (!pwrStickActive) {
            pwrStickActive = true;
#if !POT_DISABLED
            potAtCycle = smoothPot;
            powerLevel = potLevel;
#endif
        }
        powerLevel += yDev * PWR_STICK_RATE;
        if (powerLevel > 1.0f) powerLevel = 1.0f;
        if (powerLevel < 0.0f) powerLevel = 0.0f;
    }
#if !POT_DISABLED
    if (pwrStickActive && abs(smoothPot - potAtCycle) > 120) pwrStickActive = false;
    if (!pwrStickActive) powerLevel = potLevel;
#endif

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
    if (loadCalibration()) {
        useFixedCal = true;
        Serial.printf("[jsCal] loaded fixed calibration cx=%d cy=%d ld=%d rd=%d ud=%d dd=%d\n",
                      stickCenterX, stickCenterY, stickLeftDev, stickRightDev,
                      stickUpDev, stickDownDev);
    }

    // Audio samples live in flash (PROGMEM) and are read straight from there
    // by the ISR, so Audio::begin() only needs a small beep buffer — the
    // 76.8 KB video frame buffer still gets the large contiguous heap block.
    Audio::begin();

    video_graphics(NTSC_320x240, FB_FORMAT_GREY_8BPP);
    renderer = new RendererESP32(fbShadow, XRES, YRES);
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
    if (!calibrateMode) {
        while (acc >= stepMillis) {
            game.update();
            acc -= stepMillis;
        }
    }

    if (game.state != lastGameState) {
        if (game.state == STATE_CRASHED) {
            if (game.lavaBurnGet()) Audio::playBurn();
            else Audio::playExplosion();
        }
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
        Serial.printf("[nunchuck] x=%d y=%d c=%d z=%d err=%u pwr=%d stick=%d\n",
                      nunchuck.joystickX(), nunchuck.joystickY(),
                      nunchuck.buttonC(), nunchuck.buttonZ(),
                      nunchuck.readErrors(), (int)(powerLevel * 100),
                      pwrStickActive ? 1 : 0);
    }

    video_wait_frame();
    memcpy(video_get_frame_buffer_address(), fbShadow, (size_t)XRES * (size_t)YRES);
    game.draw(*renderer);
    if (calibrateMode) drawCalibration(*renderer);
}

// Overlay for joystick calibration: live stick position in a box plus the
// captured min/max range. C = save, START = cancel.
static void drawCalibration(Renderer &r)
{
    r.clear(); // leave a clean black background behind the overlay
    r.text((XRES - (int)strlen("JOYSTICK CALIBRATION") * 6) / 2.0f, 18,
           "JOYSTICK CALIBRATION");
    r.text((XRES - (int)strlen("MOVE STICK TO ALL EDGES") * 6) / 2.0f, 36,
           "MOVE STICK TO ALL EDGES");

    // Stick range box (map 0..255 onto the box). Drawn as an outline so the
    // live stick cursor stays visible inside it.
    const int bx = 110, by = 80, bw = 100, bh = 100;
    r.line(bx, by, bx + bw, by);
    r.line(bx, by, bx, by + bh);
    r.line(bx + bw, by, bx + bw, by + bh);
    r.line(bx, by + bh, bx + bw, by + bh);
    int jx = nunchuck.joystickX();
    int jy = nunchuck.joystickY();
    int px = bx + (int)(jx * (bw - 2) / 255.0f) + 1;
    int py = by + (int)((255 - jy) * (bh - 2) / 255.0f) + 1;
    r.rect(px - 2, py - 2, 5, 5);

    char buf[24];
    snprintf(buf, sizeof(buf), "x=%d y=%d", jx, jy);
    r.text((XRES - (int)strlen(buf) * 6) / 2.0f, 190, buf);
    snprintf(buf, sizeof(buf), "range X %d..%d Y %d..%d",
             calMinX, calMaxX, calMinY, calMaxY);
    r.text((XRES - (int)strlen(buf) * 6) / 2.0f, 202, buf);
    r.text((XRES - (int)strlen("C: SAVE  START: CANCEL") * 6) / 2.0f, 216,
           "C: SAVE  START: CANCEL");
}
