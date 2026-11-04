/* SPDX-License-Identifier: MIT */
#include <Arduino.h>
void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF);
}
void loop()
{
    digitalWrite(LED_BUILTIN, LED_BUILTIN_ON);
    delay(500);
    digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF);
    delay(500);
}
