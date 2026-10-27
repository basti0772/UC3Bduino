/* SPDX-License-Identifier: MIT */
#include "uc3b_system_timer.h"
#include "uc3b_clock.h"
#include "uc3b_system_timer_hw.h"

enum { TIMER_PRESCALER = 8, TIMER_HZ = 100,
       PERIOD_MS = 10, PERIOD_US = 10000 };

static volatile uint32_t epoch_ms;
static volatile uint32_t epoch_us;
static uint32_t period_ticks; /* Immutable after initialization. */

static inline void advance_period(void)
{
    /* Independent unsigned counters preserve both natural 32-bit wraps. */
    epoch_ms += PERIOD_MS;
    epoch_us += PERIOD_US;
}

void UC3B_SYSTEM_TIMER_ISR uc3b_system_timer_isr(void)
{
    if (timer_hw_take_period()) {
        advance_period();
    }
    timer_hw_ack_barrier();
}

bool uc3b_system_timer_init(void)
{
    uint32_t pba_hz = uc3b_clock_get_pba_hz();
    uint32_t ticks = pba_hz / (TIMER_PRESCALER * TIMER_HZ);
    if (period_ticks != 0 || !timer_hw_init_allowed()
        || pba_hz % (TIMER_PRESCALER * TIMER_HZ) != 0
        || ticks < 10 || ticks > 65535) {
        return false;
    }
    /* Require an exact 10 ms period; never silently round a future clock. */
    period_ticks = ticks;
    epoch_ms = 0;
    epoch_us = 0;
    timer_hw_configure(ticks);
    timer_hw_enable();
    return true;
}

static uint32_t read_time(bool microseconds)
{
    uint32_t sr, ticks, base;
    if (period_ticks == 0) {
        return 0;
    }
    sr = timer_hw_irq_save();
    ticks = timer_hw_counter();
    /* CV BEFORE the read-to-clear SR is essential. If a compare happened
     * before/during the sample, account it once and take the new period's CV.
     * If it happens AFTER SR, the original CV is a valid earlier snapshot;
     * the still-pending flag belongs to the next ISR/read. The ISR cannot
     * interleave because GM is set. Both paths use the same epoch update.
     * At most ONE unserviced compare is allowed, including this short region.
     */
    if (timer_hw_take_period()) {
        advance_period();
        ticks = timer_hw_counter();
    }
    base = microseconds ? epoch_us : epoch_ms;
    timer_hw_ack_barrier();
    timer_hw_irq_restore(sr);
    /* Convert outside the critical section. 65535 * 10000 fits uint32_t.
     * GCC emits native divu; no 64-bit arithmetic or runtime helpers needed.
     */
    return base + ticks * (microseconds ? PERIOD_US : PERIOD_MS) / period_ticks;
}

uint32_t uc3b_system_timer_millis(void)
{
    return read_time(false);
}

uint32_t uc3b_system_timer_micros(void)
{
    return read_time(true);
}
