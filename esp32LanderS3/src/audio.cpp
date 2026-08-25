#include <Arduino.h>
#include "audio.h"
#include "audio_data.h"
#include "tanker_explosion_sound.h"
#include "soc/ledc_struct.h"

#define AUDIO_PIN 18
#define AUDIO_LEDC_CHANNEL 0
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_PWM_FREQ 312500
#define AUDIO_PWM_RESOLUTION 8

// Per-tick volume easing step (INTEGER, IRAM-safe): moves current level by
// ~1/24 of the gap each sample (~30 ms attack/release at 16 kHz, no FPU).
#define ENV_DIV 24
// How strongly the wind is scaled down (subtle by design, but audible).
#define WIND_GAIN 140.0f
// Max wind volume out of 256.
#define WIND_MAX 160

// Burn voice (ship melting after a lava touchdown): the crash sample played
// quiet (~1/5 volume) with a 4 s envelope that matches CRASH_RESET_DELAY and
// random gain jitter for a fire-crackle feel.
#define BURN_LEN_SAMPLES (AUDIO_SAMPLE_RATE * 4)
#define BURN_FADE_IN 3200      // 0.2 s
#define BURN_FADE_OUT 19200    // 1.2 s
#define BURN_CROSS 2048        // loop-seam crossfade length
#define BURN_GAIN_MAX 50

static hw_timer_t *audioTimer = NULL;
static uint8_t *thrustBuf = NULL;
static uint8_t *explBuf = NULL;
static uint8_t *windBuf = NULL;
static uint8_t *boltBuf = NULL;
static uint8_t *quakeBuf = NULL;
static const uint8_t *tankerExplBuf = NULL;

static volatile uint16_t thrustPos = 0;
static volatile int thrustPrev = 0;
static volatile uint16_t explPos = 0xFFFF;
static volatile uint16_t windPos = 0;
static volatile uint16_t boltPos = 0xFFFF;
static volatile uint32_t quakePos = 0xFFFFFFFF;
static volatile uint32_t burnPos = 0xFFFFFFFF;
static volatile uint32_t burnNoise = 0xABCDEF01u;
static volatile uint32_t tankerExplPos = 0xFFFFFFFF;
static volatile int tankerExplLevel = 0;

// Targets set from the game; current levels are eased toward them in the ISR
// using integer math only (no FPU inside the IRAM ISR).
static volatile int thrustTarget = 0;
static volatile int windTarget = 0;
static volatile int thrustLevel = 0;
static volatile int windLevel = 0;

static volatile uint32_t audioIsrCount = 0;

// Integer easing toward target (IRAM/ISR-safe, no floating point).
static inline IRAM_ATTR int envEase(int cur, int tgt) {
    int diff = tgt - cur;
    if (diff == 0) return cur;
    int step = diff / ENV_DIV;
    if (step == 0) step = (diff > 0) ? 1 : -1;
    cur += step;
    if ((diff > 0 && cur > tgt) || (diff < 0 && cur < tgt)) cur = tgt;
    return cur;
}

static void IRAM_ATTR audioIsr() {
    int32_t v = 0;

    audioIsrCount++;

    // Ease engine/wind volume toward target (attack/release -> no clicks).
    thrustLevel = envEase(thrustLevel, thrustTarget);
    windLevel = envEase(windLevel, windTarget);

    int16_t lvl = (int16_t)thrustLevel;
    int16_t wlvl = (int16_t)windLevel;

    if (lvl > 0) {
        // Running one-pole low-pass (integer, ~alpha 0.5) to soften high
        // freqs — the samples now live in flash, so no pre-smoothed RAM copy.
        int s = (int32_t)(thrustBuf[thrustPos] - 128);
        thrustPrev += (s - thrustPrev) >> 1;
        v += thrustPrev * lvl >> 8;
        thrustPos++;
        if (thrustPos >= THRUST_SOUND_LEN) thrustPos = 0;
    }
    if (wlvl > 0) {
        v += (int32_t)(windBuf[windPos] - 128) * wlvl >> 8;
        windPos++;
        if (windPos >= WIND_SOUND_LEN) windPos = 0;
    }
    if (explPos < EXPLOSION_SOUND_LEN) {
        v += (int32_t)explBuf[explPos] - 128;
        explPos++;
        if (explPos >= EXPLOSION_SOUND_LEN) explPos = 0xFFFF;
    }
    if (tankerExplPos < TANKER_EXPLOSION_SOUND_LEN) {
        v += (((int32_t)pgm_read_byte(&tankerExplBuf[tankerExplPos]) - 128) * tankerExplLevel) >> 8;
        tankerExplPos++;
        if (tankerExplPos >= TANKER_EXPLOSION_SOUND_LEN) tankerExplPos = 0xFFFFFFFF;
    }
    if (boltPos < LIGHTNING_SOUND_LEN) {
        v += (int32_t)boltBuf[boltPos] - 128;
        boltPos++;
        if (boltPos >= LIGHTNING_SOUND_LEN) boltPos = 0xFFFF;
    }
    if (quakePos < QUAKE_SOUND_LEN) {
        v += (int32_t)quakeBuf[quakePos] - 128;
        quakePos++;
        if (quakePos >= QUAKE_SOUND_LEN) quakePos = 0xFFFFFFFF;
    }

    if (burnPos < BURN_LEN_SAMPLES) {
        uint32_t ph = burnPos;
        int g = BURN_GAIN_MAX;
        if (ph < BURN_FADE_IN) {
            g = g * (int)ph / BURN_FADE_IN;
        } else if (ph > (uint32_t)(BURN_LEN_SAMPLES - BURN_FADE_OUT)) {
            uint32_t rem = BURN_LEN_SAMPLES - ph;
            g = g * (int)rem / BURN_FADE_OUT;
        }
        // Random gain jitter -> fire crackle (integer LFSR, IRAM-safe).
        burnNoise ^= burnNoise << 13;
        burnNoise ^= burnNoise >> 17;
        burnNoise ^= burnNoise << 5;
        int jit = (int)(burnNoise & 0x7F) - 64;
        g += jit * g / 256;
        if (g < 0) g = 0;
        if (g > 255) g = 255;

        uint32_t k = ph % EXPLOSION_SOUND_LEN;
        int32_t s = (int32_t)explBuf[k] - 128;
        if (k < BURN_CROSS && ph >= EXPLOSION_SOUND_LEN) {
            // Blend the just-played quiet tail into the loud head so the
            // loop seam never clicks.
            uint32_t pk = k + EXPLOSION_SOUND_LEN - BURN_CROSS;
            int32_t ps = (int32_t)explBuf[pk] - 128;
            int mix = (int)(k * 255 / BURN_CROSS);
            s = (s * mix + ps * (255 - mix)) >> 8;
        }
        v += (s * g) >> 8;
        burnPos++;
    }

    if (v < -128) v = -128;
    else if (v > 127) v = 127;

    uint32_t duty = (uint32_t)((uint8_t)(v + 128)) << 4;
    ledc_dev_t *hw = &LEDC;
    hw->channel_group[LEDC_LOW_SPEED_MODE].channel[0].duty.duty = duty;
    hw->channel_group[LEDC_LOW_SPEED_MODE].channel[0].conf1.val =
        hw->channel_group[LEDC_LOW_SPEED_MODE].channel[0].conf1.val | (1U << 31);
    hw->channel_group[LEDC_LOW_SPEED_MODE].channel[0].conf0.low_speed_update = 1;
}

void Audio::begin() {
    Serial.println("[audio] begin");

    bool cs = ledcSetClockSource(LEDC_USE_APB_CLK);
    Serial.printf("[audio] ledcSetClockSource(APB): %d\n", (int)cs);

    bool ok = ledcAttachChannel(AUDIO_PIN, AUDIO_PWM_FREQ, AUDIO_PWM_RESOLUTION, AUDIO_LEDC_CHANNEL);
    Serial.printf("[audio] ledcAttachChannel: %d\n", (int)ok);

    bool wr = ledcWriteChannel(AUDIO_LEDC_CHANNEL, 128);
    Serial.printf("[audio] ledcWriteChannel(128): %d\n", (int)wr);
    delay(1500);
    ledcWriteChannel(AUDIO_LEDC_CHANNEL, 0);

    // Samples stay in FLASH (PROGMEM, memory-mapped) and are read straight
    // from there by the ISR. The game never writes flash at runtime, so the
    // data cache in the ISR is safe, and no heap is consumed for sample RAM —
    // the 76.8 KB video frame buffer needs that memory.
    thrustBuf = (uint8_t *)THRUST_SOUND;
    explBuf = (uint8_t *)EXPLOSION_SOUND;
    windBuf = (uint8_t *)WIND_SOUND;
    boltBuf = (uint8_t *)LIGHTNING_SOUND;
    quakeBuf = (uint8_t *)QUAKE_SOUND;
    tankerExplBuf = TANKER_EXPLOSION_SOUND;

    audioTimer = timerBegin(AUDIO_SAMPLE_RATE);
    timerAttachInterrupt(audioTimer, audioIsr);
    timerAlarm(audioTimer, 1, true, 0);
    Serial.println("[audio] timer started");
}

void Audio::setThrust(float level) {
    if (level <= 0.0f) {
        thrustTarget = 0;
        return;
    }
    if (level > 1.0f) level = 1.0f;
    thrustTarget = (int)(level * 256.0f);
}

void Audio::setWind(float level) {
    if (level <= 0.0f) {
        windTarget = 0;
        return;
    }
    if (level > 1.0f) level = 1.0f;
    // Subtle, and proportional to wind strength.
    float v = WIND_GAIN * level;
    if (v > WIND_MAX) v = WIND_MAX;
    windTarget = (int)v;
}

void Audio::playExplosion() {
    explPos = 0;
}

void Audio::playTankerExplosion() {
    tankerExplPos = 0;
    tankerExplLevel = 255;
}

void Audio::playBurn() {
    burnPos = 0;
}

void Audio::playLightning() {
    boltPos = 0;
}

void Audio::playQuake() {
    quakePos = 0;
}

uint32_t Audio::debugIsrCount() {
    return audioIsrCount;
}

void Audio::debugRegs() {
    auto &ch = LEDC.channel_group[LEDC_LOW_SPEED_MODE].channel[0];
    auto &tm = LEDC.timer_group[LEDC_LOW_SPEED_MODE].timer[0];
    Serial.printf("[audio] ch conf0=%08X conf1=%08X duty=%08X duty_rd=%u | "
                  "timer res=%u div=%u tick=%u cnt=%u\n",
                  (unsigned)ch.conf0.val, (unsigned)ch.conf1.val,
                  (unsigned)ch.duty.val, (unsigned)ch.duty_rd.duty_read,
                  (unsigned)tm.conf.duty_resolution, (unsigned)tm.conf.clock_divider,
                  (unsigned)tm.conf.tick_sel, (unsigned)tm.value.timer_cnt);
}
