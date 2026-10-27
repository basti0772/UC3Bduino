/* SPDX-License-Identifier: MIT */
#include <Arduino.h>
#if !UC3BDUINO_TEST_PIN_CONFIGURED
#error "Configure logical D0 from the real board schematic before building Blink"
#endif
const uint32_t TEST_PIN = D0; /* No assumed LED. */
void setup()
{
    pinMode(TEST_PIN, OUTPUT);
}
void loop()
{
    digitalWrite(TEST_PIN, HIGH);
    delay(500);
    digitalWrite(TEST_PIN, LOW);
    delay(500);
}
