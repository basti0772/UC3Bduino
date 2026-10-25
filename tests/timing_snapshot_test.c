/* SPDX-License-Identifier: MIT
 * Execute the real portable timer logic with deterministic register-read events.
 * This does not simulate AVR32 instructions, INTC latency or physical hardware.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define UC3B_SYSTEM_TIMER_HW_H /* Replace only the private hardware adapter. */
#define UC3B_SYSTEM_TIMER_ISR
static uint32_t fake_pba = 115200;
static uint32_t fake_gm = 1, configured_ticks, enables;
static uint32_t cv, cv_after_status;
static bool pending, compare_at_status, compare_after_status;
static unsigned reads;
uint32_t uc3b_clock_get_pba_hz(void) { return fake_pba; }
static uint32_t timer_hw_irq_save(void)
{
    uint32_t old = fake_gm;
    fake_gm = 1;
    return old;
}
static void timer_hw_irq_restore(uint32_t old) { fake_gm = old; }
static bool timer_hw_init_allowed(void) { return fake_gm != 0; }
static uint32_t timer_hw_counter(void) { ++reads; return cv; }
static bool timer_hw_take_period(void)
{
    bool result;
    assert(fake_gm); /* ISR tests also model its hardware interrupt mask. */
    if (compare_at_status) { pending = true; cv = cv_after_status; }
    result = pending;
    pending = false;
    if (compare_after_status) { pending = true; cv = cv_after_status; }
    return result;
}
static void timer_hw_ack_barrier(void) { }
static void timer_hw_configure(uint32_t ticks) { configured_ticks = ticks; }
static void timer_hw_enable(void) { ++enables; fake_gm = 0; }

#include "../src/hal/uc3b_system_timer.c"

static void reset_sample(uint32_t count)
{
    cv = count;
    pending = false;
    compare_at_status = compare_after_status = false;
    reads = 0;
}

int main(void)
{
    uint32_t i;
    assert(uc3b_system_timer_millis() == 0);
    fake_gm = 0;
    assert(!uc3b_system_timer_init());
    fake_gm = 1;
    fake_pba = 115201; /* No silently rounded timing. */
    assert(!uc3b_system_timer_init());
    fake_pba = 60000000; /* PBA/8 / 100 exceeds 16-bit RC. */
    assert(!uc3b_system_timer_init());
    fake_pba = 0;
    assert(!uc3b_system_timer_init());
    assert(configured_ticks == 0 && enables == 0);
    fake_pba = 115200;
    assert(uc3b_system_timer_init());
    assert(configured_ticks == 144 && enables == 1 && fake_gm == 0);
    assert(!uc3b_system_timer_init());

    /* Every current-clock counter position, plus independent 64-bit oracles
     * around each API's wrap. Production only uses 32-bit arithmetic.
     */
    epoch_us = UINT32_MAX - 9999;
    epoch_ms = UINT32_MAX - 4;
    for (i = 0; i < 144; ++i) {
        reset_sample(i);
        assert(uc3b_system_timer_micros()
               == (uint32_t)((uint64_t)epoch_us + (uint64_t)i * 1000000 / 14400));
        assert(uc3b_system_timer_millis()
               == (uint32_t)((uint64_t)epoch_ms + (uint64_t)i * 1000 / 14400));
        assert(fake_gm == 0);
    }
    epoch_ms = 1230; epoch_us = 1230000;
    reset_sample(143);
    compare_at_status = true; cv_after_status = 1;
    assert(uc3b_system_timer_micros() == 1240069);
    assert(epoch_ms == 1240 && epoch_us == 1240000 && reads == 2);
    compare_at_status = false;
    fake_gm = 1;
    uc3b_system_timer_isr(); /* Cleared snapshot must not double-count. */
    assert(epoch_ms == 1240);
    fake_gm = 0;

    reset_sample(143);
    compare_after_status = true; cv_after_status = 0;
    assert(uc3b_system_timer_micros() == 1249930); /* Earlier, coherent sample. */
    assert(epoch_ms == 1240 && pending && reads == 1);
    compare_after_status = false;
    fake_gm = 1;
    uc3b_system_timer_isr();
    assert(epoch_ms == 1250 && !pending);
    assert(uc3b_system_timer_micros() == 1250000);
    assert(fake_gm == 1); /* A caller's masked state stays masked. */

    epoch_ms = UINT32_MAX - 5; epoch_us = UINT32_MAX - 5000;
    reset_sample(2); pending = true;
    assert(uc3b_system_timer_millis() == 4);
    assert(epoch_us == 4999 && epoch_ms == 4);
    assert(uc3b_system_timer_micros() == 5137);

    /* Future valid clocks: exhaustive CV coverage at the largest supported RC. */
    period_ticks = 65535;
    epoch_ms = 0; epoch_us = 0;
    for (i = 0; i < period_ticks; ++i) {
        reset_sample(i);
        assert(uc3b_system_timer_micros() == (uint32_t)((uint64_t)i * 10000 / 65535));
        assert(uc3b_system_timer_millis() == (uint32_t)((uint64_t)i * 10 / 65535));
    }
    puts("PASS: timer snapshots, pending compares, wraps, mask preservation and conversion bounds");
    return 0;
}
