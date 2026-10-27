/* SPDX-License-Identifier: MIT
 * Execute the real ADC state machine against fake registers and CPU COUNT.
 * This verifies sequencing/timeouts, not analog behavior or electrical accuracy.
 */
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#define UC3B_ADC_HW_H

static uint32_t cpu, pba, count, step, irq_mask, enabled, eoc;
static uint32_t enables, resets, configurations, starts, prescale, polls, accesses;
static uint32_t ready_after, conversion_polls, count_reads;
static bool clock_ready, converting, stuck, frozen, nested;
static uint16_t data[8];
static uint16_t conversion_value;
static uint32_t expected_irq;

uint32_t uc3b_clock_get_cpu_hz(void) { return cpu; }
uint32_t uc3b_clock_get_pba_hz(void) { return pba; }
static uint32_t adc_hw_irq_save(void) { uint32_t old = irq_mask; irq_mask = 1; return old; }
static void adc_hw_irq_restore(uint32_t old) { irq_mask = old; }
static uint32_t adc_hw_count(void)
{
    assert(irq_mask == expected_irq);
    ++count_reads;
    if (!frozen) { count += step; }
    return count;
}
static void adc_hw_enable_clock(void) { ++enables; ++accesses; }
static bool adc_hw_clock_ready(void) { ++accesses; return clock_ready; }
static void adc_hw_reset(void)
{
    ++resets; ++accesses; converting = false; enabled = eoc = 0;
}
static void adc_hw_configure(uint32_t prescaler)
{
    ++configurations; ++accesses; prescale = prescaler;
    assert(!converting);
}
static void adc_hw_disable_channels(void)
{
    assert(!converting); /* Changing CHDR during conversion is forbidden. */
    ++accesses; enabled = 0;
}
static void adc_hw_enable_channel(uint8_t channel)
{
    assert(!converting && enabled == 0);
    ++accesses; enabled = 1UL << channel;
}
static uint16_t adc_hw_read(uint8_t channel)
{
    ++accesses; eoc &= ~(1UL << channel); return data[channel];
}
static void adc_hw_clear_last(void) { ++accesses; }
static void adc_hw_start(void)
{
    assert(enabled != 0 && (enabled & (enabled - 1)) == 0);
    assert((eoc & enabled) == 0); /* An old CDR must not satisfy this wait. */
    ++starts; ++accesses; converting = true; conversion_polls = 0;
}
static bool adc_hw_complete(uint8_t channel);
#include "../src/hal/uc3b_adc.c"

static bool adc_hw_complete(uint8_t channel)
{
    ++polls; ++accesses; ++conversion_polls;
    assert(irq_mask == expected_irq && enabled == (1UL << channel));
    if (nested) {
        uint16_t value = 999;
        uint32_t old_accesses = accesses;
        nested = false;
        assert(!uc3b_adc_read_channel(7, &value));
        assert(value == 999 && accesses == old_accesses && adc_busy);
    }
    if (!stuck && conversion_polls >= ready_after) {
        converting = false; data[channel] = conversion_value;
        eoc |= 1UL << channel;
    }
    return (eoc & (1UL << channel)) != 0;
}

static void reset_fixture(uint32_t frequency)
{
    uint8_t channel;
    cpu = pba = frequency; count = UINT32_MAX - 70; step = 11;
    irq_mask = expected_irq = enabled = eoc = 0;
    enables = resets = configurations = starts = prescale = polls = accesses = 0;
    count_reads = conversion_polls = 0; ready_after = 3;
    clock_ready = true; converting = stuck = frozen = nested = false;
    conversion_value = 512; adc_initialized = adc_busy = false;
    for (channel = 0; channel < 8; ++channel) { data[channel] = 42; }
}

int main(void)
{
    const uint32_t frequencies[] = { 115200, 400000, 8000000, 12000000, 16000000, 60000000 };
    const uint32_t dividers[] = { 1, 1, 2, 3, 4, 12 };
    uint32_t i, old_accesses, old_resets;
    uint16_t value = 777;
    uint8_t channel;
    reset_fixture(12000000);
    assert(!uc3b_adc_read_channel(8, &value));
    assert(!uc3b_adc_read_channel(255, &value));
    assert(!uc3b_adc_read_channel(0, 0));
    assert(accesses == 0 && count_reads == 0 && value == 777);
    for (i = 0; i < sizeof(frequencies) / sizeof(frequencies[0]); ++i) {
        reset_fixture(frequencies[i]);
        assert(uc3b_adc_read_channel(0, &value) && value == 512);
        assert(prescale + 1 == dividers[i]);
        assert(pba <= 5000000UL * (prescale + 1));
        assert(count_reads * step >= 256UL * dividers[i]);
        assert(enables == 1 && configurations == 1 && starts == 1);
        for (channel = 0; channel < 8; ++channel) {
            data[channel] = 42; eoc = 1UL << channel; // stale completion
            conversion_value = (uint16_t)(channel * 146 + 1);
            assert(uc3b_adc_read_channel(channel, &value));
            assert(value == conversion_value && enabled == 0 && eoc == 0);
        }
        assert(configurations == 1 && resets == 1 && starts == 9);
        conversion_value = 0; nested = true;
        assert(uc3b_adc_read_channel(7, &value) && value == 0);
        conversion_value = 1023;
        assert(uc3b_adc_read_channel(0, &value) && value == 1023);
        assert(!adc_busy && irq_mask == 0);
    }
    reset_fixture(12000000);
    expected_irq = irq_mask = 1; // preserve a caller's mask, even during waits
    assert(uc3b_adc_read_channel(3, &value) && irq_mask == 1);
    stuck = true; value = 777; old_resets = resets;
    assert(!uc3b_adc_read_channel(4, &value));
    assert(value == 777 && resets == old_resets + 1 && !adc_initialized && !adc_busy);
    assert(!converting && enabled == 0 && irq_mask == 1);
    stuck = false;
    assert(uc3b_adc_read_channel(5, &value) && configurations == 2);
    frozen = stuck = true; old_accesses = accesses;
    assert(!uc3b_adc_read_channel(6, &value));
    assert(accesses - old_accesses < ADC_POLL_LIMIT + 20 && !adc_initialized);
    reset_fixture(115200);
    frozen = true;
    assert(!uc3b_adc_read_channel(0, &value)); // frozen COUNT during startup
    assert(starts == 0 && !adc_initialized && resets == 2);
    reset_fixture(12000000);
    clock_ready = false;
    assert(!uc3b_adc_read_channel(0, &value));
    assert(resets == 0 && configurations == 0 && starts == 0 && !adc_busy);
    frozen = true;
    assert(!uc3b_adc_read_channel(0, &value)); // bounded clock-ready wait, too
    assert(resets == 0);
    reset_fixture(12000000);
    pba = 0;
    assert(!uc3b_adc_read_channel(0, &value) && accesses == 0);
    pba = 12000001;
    assert(!uc3b_adc_read_channel(0, &value) && accesses == 0);
    pba = cpu = 60000001;
    assert(!uc3b_adc_read_channel(0, &value) && accesses == 0);
    reset_fixture(60000000);
    pba = 115200;
    assert(uc3b_adc_read_channel(7, &value)); // divided bus, rounded-up startup
    puts("PASS: ADC clocks, startup, channel switching, stale EOC, bounds, recovery and IRQ masks (fake hardware)");
    return 0;
}
