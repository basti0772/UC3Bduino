/* SPDX-License-Identifier: MIT */
#include <Arduino.h>
#if !UC3BDUINO_TEST_PIN_CONFIGURED
#error "Configure logical D0 from the real board schematic before building MicrosPulse"
#endif
const uint32_t TEST_PIN = D0;
/* At nominal 115.2 kHz CPU, software overhead dominates 100 us pulses.
 * Start with measurable 10/90 ms waits; see TIMING.md before shortening them.
 */
const unsigned int HIGH_US = 10000U;
const unsigned int LOW_US = 90000U;
volatile uint32_t lastHighElapsedUs; /* Inspect with a debugger; no Serial yet. */
void setup()
{
    pinMode(TEST_PIN, OUTPUT);
}
void loop()
{
    digitalWrite(TEST_PIN, HIGH);
    uint32_t started = micros();
    delayMicroseconds(HIGH_US);
    lastHighElapsedUs = (uint32_t)(micros() - started);
    digitalWrite(TEST_PIN, LOW);
    delayMicroseconds(LOW_US);
}
