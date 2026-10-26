/* SPDX-License-Identifier: MIT
 * Execute the real Arduino wrappers with a progressing, wrapping fake clock.
 * This checks wait arithmetic, not physical timing accuracy.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "hal/uc3b_system_timer.h"

static uint64_t elapsed;
static uint32_t now, step, reads;
static uint32_t next_time(void)
{
    uint32_t result = now;
    now += step;
    elapsed += step;
    ++reads;
    assert(reads < 20000000UL);
    return result;
}
uint32_t uc3b_system_timer_millis(void) { return next_time(); }
uint32_t uc3b_system_timer_micros(void) { return next_time(); }

#include "../arduino/hardware/uc3bduino/avr32/cores/uc3bduino/wiring_time.cpp"

static void reset_clock(uint32_t increment)
{
    now = UINT32_MAX - 7;
    elapsed = 0;
    reads = 0;
    step = increment;
}

int main(void)
{
    const uint32_t waits[] = { 1, 7, 8, 9, 999, 1000, 1001, 9999, 10000,
                               10001, UINT32_MAX };
    unsigned int i;
    reset_clock(1);
    delay(0); delayMicroseconds(0);
    assert(reads == 0);
    assert(millis() == UINT32_MAX - 7);
    assert(micros() == UINT32_MAX - 6);
    for (i = 0; i < sizeof(waits) / sizeof(waits[0]); ++i) {
        uint32_t value = waits[i];
        reset_clock(value == UINT32_MAX ? 1000 : 1);
        delay(value);
        assert(elapsed >= value);
        reset_clock(value == UINT32_MAX ? 10000 : 1);
        delayMicroseconds(value);
        assert(elapsed >= value);
    }
    puts("PASS: real timing wrappers, zero waits, wrap crossings, chunk limits and UINT32_MAX waits");
    return 0;
}
