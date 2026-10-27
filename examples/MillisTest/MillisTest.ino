/* SPDX-License-Identifier: MIT */
#include <Arduino.h>
#if !UC3BDUINO_TEST_PIN_CONFIGURED
#error "Configure logical D0 from the real board schematic before building MillisTest"
#endif
const uint32_t TEST_PIN = D0;
uint32_t changedAt;
bool highPhase;
void setup()
{
    pinMode(TEST_PIN, OUTPUT);
    changedAt = millis();
    highPhase = false;
}
void loop()
{
    uint32_t now = millis();
    uint32_t interval = highPhase ? 250UL : 750UL;
    if ((uint32_t)(now - changedAt) >= interval) {
        highPhase = !highPhase;
        digitalWrite(TEST_PIN, highPhase ? HIGH : LOW);
        changedAt = now;
    }
}
