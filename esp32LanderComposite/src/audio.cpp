#include <Arduino.h>
#include "audio.h"
#include "audio_data.h"
#include "soc/ledc_struct.h"
#include "driver/dac.h"
#include "driver/rtc_io.h"

#define AUDIO_PIN 26
#define AUDIO_LEDC_CHANNEL 0
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_PWM_FREQ 312500
#define AUDIO_PWM_RESOLUTION 8
#define BEEP_LEN 8000

static hw_timer_t *audioTimer = NULL;
static uint8_t *thrustBuf = NULL;
static uint8_t *explBuf = NULL;
static uint8_t *beepBuf = NULL;

static volatile uint16_t thrustPos = 0;
static volatile uint16_t explPos = 0xFFFF;
static volatile uint16_t thrustLevel = 0;
static volatile uint16_t beepPos = BEEP_LEN;
static volatile uint16_t beepLen = 0;
static volatile uint32_t audioIsrCount = 0;

static void IRAM_ATTR audioIsr() {
    int32_t v = 0;

    audioIsrCount++;

    if (thrustLevel > 0) {
        v += (int32_t)(thrustBuf[thrustPos] - 128) * (int32_t)thrustLevel >> 8;
        thrustPos++;
        if (thrustPos >= THRUST_SOUND_LEN) thrustPos = 0;
    }
    if (explPos < EXPLOSION_SOUND_LEN) {
        v += (int32_t)explBuf[explPos] - 128;
        explPos++;
        if (explPos >= EXPLOSION_SOUND_LEN) explPos = 0xFFFF;
    }
    if (beepPos < beepLen) {
        v += (int32_t)beepBuf[beepPos] - 128;
        beepPos++;
    }

    if (v < -128) v = -128;
    else if (v > 127) v = 127;

    uint32_t duty = (uint32_t)((uint8_t)(v + 128)) << 4;
    ledc_dev_t *hw = &LEDC;
    hw->channel_group[LEDC_HIGH_SPEED_MODE].channel[0].duty.duty = duty;
    hw->channel_group[LEDC_HIGH_SPEED_MODE].channel[0].conf1.duty_start = 1;
}

void Audio::begin() {
    Serial.println("[audio] begin");

    esp_err_t e = dac_output_disable(DAC_CHANNEL_2);
    Serial.printf("[audio] dac_output_disable: %d\n", (int)e);
    e = rtc_gpio_deinit(GPIO_NUM_26);
    Serial.printf("[audio] rtc_gpio_deinit: %d\n", (int)e);

    bool ok = ledcAttachChannel(AUDIO_PIN, AUDIO_PWM_FREQ, AUDIO_PWM_RESOLUTION, AUDIO_LEDC_CHANNEL);
    Serial.printf("[audio] ledcAttachChannel: %d\n", (int)ok);

    bool wr = ledcWriteChannel(AUDIO_LEDC_CHANNEL, 128);
    Serial.printf("[audio] ledcWriteChannel(128): %d (measure ~1.65V on pin 26)\n", (int)wr);
    delay(1500);
    ledcWriteChannel(AUDIO_LEDC_CHANNEL, 0);

    thrustBuf = (uint8_t *)malloc(THRUST_SOUND_LEN);
    explBuf = (uint8_t *)malloc(EXPLOSION_SOUND_LEN);
    beepBuf = (uint8_t *)malloc(BEEP_LEN);
    memcpy_P(thrustBuf, THRUST_SOUND, THRUST_SOUND_LEN);
    memcpy_P(explBuf, EXPLOSION_SOUND, EXPLOSION_SOUND_LEN);
    for (int i = 0; i < BEEP_LEN; i++) {
        float t = (float)i / (float)AUDIO_SAMPLE_RATE;
        float s = 0.5f + 0.4f * sinf(2.0f * PI * 440.0f * t);
        beepBuf[i] = (uint8_t)(s * 255.0f);
    }
    Serial.printf("[audio] buffers: thrust=%p expl=%p beep=%p\n", (void *)thrustBuf, (void *)explBuf, (void *)beepBuf);

    audioTimer = timerBegin(AUDIO_SAMPLE_RATE);
    timerAttachInterrupt(audioTimer, audioIsr);
    timerAlarm(audioTimer, 1, true, 0);
    Serial.println("[audio] timer started, debugBeep follows");
    debugBeep();
}

void Audio::debugBeep() {
    beepLen = BEEP_LEN;
    beepPos = 0;
}

void Audio::setThrust(float level) {
    if (level <= 0.0f) {
        thrustLevel = 0;
        return;
    }
    if (level > 1.0f) level = 1.0f;
    thrustLevel = (uint16_t)(level * 256.0f);
}

void Audio::playExplosion() {
    explPos = 0;
}

uint32_t Audio::debugIsrCount() {
    return audioIsrCount;
}
