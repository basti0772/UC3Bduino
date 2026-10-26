/* SPDX-License-Identifier: MIT */
#include <Arduino.h>
#include "hal/uc3b_system_timer.h"

typedef char time_api_is_32_bit[(sizeof(unsigned long) == 4
                               && sizeof(unsigned int) == 4) ? 1 : -1];

unsigned long millis(void)
{
    return uc3b_system_timer_millis();
}

unsigned long micros(void)
{
    return uc3b_system_timer_micros();
}

void delay(unsigned long ms)
{
    if (ms == 0) {
        return;
    }
    /* Chunk long waits so polling can never skip the narrow exit window at
     * UINT32_MAX. Unsigned subtraction handles a wrap within each chunk.
     */
    while (ms != 0) {
        uint32_t chunk = ms > 1000UL ? 1000UL : (uint32_t)ms;
        uint32_t start = uc3b_system_timer_millis();
        while ((uint32_t)(uc3b_system_timer_millis() - start) < chunk) {
        }
        ms -= chunk;
    }
}

void delayMicroseconds(unsigned int us)
{
    if (us == 0) {
        return;
    }
    /* Timer-observed waiting, not an instruction-count calibration. Use the
     * actual PBA-derived timebase; leave the caller's interrupt state intact.
     * Quantization, call/GPIO overhead and ISR latency extend short pulses.
     */
    while (us != 0) {
        uint32_t chunk = us > 10000U ? 10000U : (uint32_t)us;
        uint32_t start = uc3b_system_timer_micros();
        while ((uint32_t)(uc3b_system_timer_micros() - start) < chunk) {
        }
        us -= chunk;
    }
}
