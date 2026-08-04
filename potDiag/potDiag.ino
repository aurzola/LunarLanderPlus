#include <Arduino.h>

const int PIN_TRIGGER = 35;

void setup()
{
    Serial.begin(115200);
    pinMode(PIN_TRIGGER, INPUT);
    delay(300);
    Serial.println("TRIG_DIAG_START");
}

void loop()
{
    int raw = analogRead(PIN_TRIGGER);
    Serial.printf("t=%d\n", raw);
    delay(5);
}
