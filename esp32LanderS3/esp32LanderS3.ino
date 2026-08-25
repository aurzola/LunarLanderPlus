#include <Arduino.h>
#include <Preferences.h>
#include <esp_pm.h>
#include <esp_random.h>
#include <sys/time.h>

#include "src/video_s3.h"
#include "src/config.h"
#include "src/game.h"
#include "src/renderer_s3.h"
#include "src/bglayer.h"
#include "src/audio.h"
#include "src/nunchuck.h"

const int XRES = (int)SCREEN_W;
const int YRES = (int)SCREEN_H;

const int PIN_POT = 8;
const int PIN_START = 13;

#define CONTROLS_WIRED 1
#define NUNCHUCK_TRIGGER_Z 1
#define POT_DISABLED 1

#define AUDIO_BRINGUP 1
#define AUDIO_TEST_TONE 0

static Game game;
static RendererS3 *renderer = NULL;

static Nunchuck nunchuck;
static bool motorOn = false;
static int btnDbStable = HIGH, btnDbPrev = HIGH;
static unsigned long btnDbTime = 0;
static int lastGameState = -1;
static unsigned long lastIsrPrint = 0;
static unsigned long lastNunchuckPrint = 0;
static unsigned long lastPerfPrint = 0;
static unsigned long fieldCount = 0;
static uint32_t drawUsMax = 0;
static uint32_t drawUsSum = 0;
static uint32_t drawUsN = 0;
static bool bgPrinted = false;

static const float PWR_STICK_RATE = 0.008f;
static bool pwrStickActive = false;
static float powerLevel = 0.5f;

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

static bool calibrateMode = false;
static bool useFixedCal = false;
static int calMinX = 255, calMaxX = 0;
static int calMinY = 255, calMaxY = 0;
static unsigned long calEnterAt = 0;
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

static void drawCalibration(Renderer &r);

static void readInputs()
{
    game.input.startPressed = false;
    game.input.chuteToggle = false;

    int btnRaw = digitalRead(PIN_START);
    unsigned long nowMs = millis();
    if (btnRaw != btnDbStable) { btnDbTime = nowMs; btnDbStable = btnRaw; }
    bool btnFell = false;
    if (nowMs - btnDbTime >= 30) {
        if (btnDbPrev == HIGH && btnDbStable == LOW) btnFell = true;
        btnDbPrev = btnDbStable;
    }

#if CONTROLS_WIRED
    nunchuck.read();

    if (calibrateMode) {
        int jx = nunchuck.joystickX();
        int jy = nunchuck.joystickY();
        if (jx < calMinX) calMinX = jx;
        if (jx > calMaxX) calMaxX = jx;
        if (jy < calMinY) calMinY = jy;
        if (jy > calMaxY) calMaxY = jy;
        if (btnFell) { cancelCalibration(); return; }
        if (nunchuck.buttonC() && !nunchuck.buttonZ()) confirmCalibration();
        return;
    }

    if (game.state == STATE_WAITING) {
        if (nunchuck.buttonC() && nunchuck.buttonZ()) {
            if (calEnterAt == 0) calEnterAt = millis();
            else if (millis() - calEnterAt >= CAL_HOLD_MS) { beginCalibration(); return; }
        } else {
            calEnterAt = 0;
        }
    }

    bool bothNow = nunchuck.buttonC() && nunchuck.buttonZ();

    game.input.angle = readStickAngle();

    bool cNow = nunchuck.buttonC();
    float yDev = readStickYDev();
    if (cNow && !bothNow && (yDev > 0.1f || yDev < -0.1f)) {
        if (!pwrStickActive) {
            pwrStickActive = true;
        }
        powerLevel += yDev * PWR_STICK_RATE;
        if (powerLevel > 1.0f) powerLevel = 1.0f;
        if (powerLevel < 0.0f) powerLevel = 0.0f;
    }

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

    if (btnFell) game.input.startPressed = true;
}

void setup()
{
    Serial.begin(115200);
    srand(esp_random());

    bgSetAllocator(ps_malloc);

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

#if AUDIO_BRINGUP
    Audio::begin();
#endif

    video_graphics_s3();
    delay(50);
    renderer = new RendererS3(video_get_frame_buffer_address(), XRES, YRES);
    Serial.printf("[lander] up %dx%d\n", XRES, YRES);
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

#if AUDIO_BRINGUP
    if (game.state != lastGameState) {
        if (game.state == STATE_CRASHED) {
            if (game.lavaBurnGet()) Audio::playBurn();
            else if (game.tankerCrashGet()) Audio::playTankerExplosion();
            else Audio::playExplosion();
        }
        lastGameState = game.state;
    }
#if AUDIO_TEST_TONE
    Audio::setThrust(0.85f);
#else
    Audio::setThrust(game.state == STATE_PLAYING ? game.ship.thrustBuild : 0.0f);
#endif
    Audio::setWind(game.windEnabled && game.state == STATE_PLAYING
                   ? game.windStrength : 0.0f);
    if (game.storm.takeNewBolt()) Audio::playLightning();
    if (game.quake.justRumbled()) Audio::playQuake();

    if (millis() - lastIsrPrint > 1000) {
        static uint32_t lastIsr = 0;
        lastIsrPrint = millis();
        uint32_t isr = Audio::debugIsrCount();
        Serial.printf("[audio] isr=%u rate=%u\n", (unsigned)isr,
                      (unsigned)(isr - lastIsr));
        lastIsr = isr;
    }
#endif

    if (millis() - lastNunchuckPrint > 500) {
        lastNunchuckPrint = millis();
        Serial.printf("[nunchuck] x=%d y=%d c=%d z=%d err=%u pwr=%d stick=%d\n",
                      nunchuck.joystickX(), nunchuck.joystickY(),
                      nunchuck.buttonC(), nunchuck.buttonZ(),
                      nunchuck.readErrors(), (int)(powerLevel * 100),
                      pwrStickActive ? 1 : 0);
    }

    video_wait_frame();
    uint32_t t0 = micros();
    game.draw(*renderer);
    if (!bgPrinted && game.bgActive()) {
        bgPrinted = true;
        Serial.println("[bg] world layer active");
    }
    uint32_t drawUs = (uint32_t)(micros() - t0);
    if (drawUs > drawUsMax) drawUsMax = drawUs;
    drawUsSum += drawUs;
    drawUsN++;
    if (calibrateMode) drawCalibration(*renderer);
    fieldCount++;

    if (millis() - lastPerfPrint > 5000) {
        lastPerfPrint = millis();
        Serial.printf("[perf] fields=%lu compose=%u us drawAvg=%u drawMax=%u\n",
                      (unsigned long)fieldCount,
                      (unsigned)video_last_compose_us(),
                      (unsigned)(drawUsN ? drawUsSum / drawUsN : 0),
                      (unsigned)drawUsMax);
        drawUsMax = 0;
        drawUsSum = 0;
        drawUsN = 0;
    }
}

static void drawCalibration(Renderer &r)
{
    r.clear();
    r.text((XRES - (int)strlen("JOYSTICK CALIBRATION") * 6) / 2.0f, 18,
           "JOYSTICK CALIBRATION");
    r.text((XRES - (int)strlen("MOVE STICK TO ALL EDGES") * 6) / 2.0f, 36,
           "MOVE STICK TO ALL EDGES");

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
