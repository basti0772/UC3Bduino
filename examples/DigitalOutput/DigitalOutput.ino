/* SPDX-License-Identifier: MIT */
#include <Arduino.h>

#if !UC3BDUINO_TEST_PIN_CONFIGURED
#error "Configure logical D0 using install-arduino-hardware.ps1 -GpioPort A|B -GpioPin number (check board wiring first)"
#endif

/* C++98: logical test slot, NOT an assumed LED or physical pin number. */
const uint32_t TEST_PIN = D0;

void setup()
{
    pinMode(TEST_PIN, OUTPUT);
}

void loop()
{
    digitalWrite(TEST_PIN, HIGH);
    digitalWrite(TEST_PIN, LOW);
    /* Fast toggle for a scope/logic analyzer; no timing or delay API yet. */
}
