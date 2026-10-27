/* SPDX-License-Identifier: MIT */
#include "uc3b_adc.h"
#include "uc3b_clock.h"
#include "uc3b_adc_hw.h"

/* Bounded even if COUNT unexpectedly stops. Normal timeouts use CPU cycles,
 * independent of timer IRQ delivery; waits retain the caller's IRQ mask.
 */
#define ADC_POLL_LIMIT 100000UL
static bool adc_initialized;
static volatile bool adc_busy;

static bool adc_init(uint32_t cpu_hz, uint32_t pba_hz, uint32_t timeout)
{
    uint32_t start, remaining, divider, startup_cycles;
    /* Ceiling division keeps ADC <= 2.5 MHz (10-bit limit is 5 MHz).
     * With PBA <= 60 MHz, PRESCAL <= 11, inside even the /128 prose limit.
     */
    divider = (pba_hz + 4999999UL) / 5000000UL;
    adc_hw_enable_clock();
    start = adc_hw_count();
    remaining = ADC_POLL_LIMIT;
    while (!adc_hw_clock_ready()) {
        if (--remaining == 0 || (uint32_t)(adc_hw_count() - start) >= timeout) {
            return false; /* Do not access ADC registers without its bus clock. */
        }
    }
    adc_hw_reset();
    adc_hw_disable_channels();
    adc_hw_configure(divider - 1);

    /* Also wait explicitly in normal mode before the first START: 128 ADC
     * clocks, >= 51.2 us at 2.5 MHz, covering the cell's 20 us startup.
     * ceil(CPU/PBA) conservatively rounds up for divided buses. Bounds in the
     * caller make the arithmetic fit uint32_t. No external VREF settling claim.
     */
    startup_cycles = ((cpu_hz + pba_hz - 1) / pba_hz) * 256UL * divider;
    start = adc_hw_count();
    remaining = ADC_POLL_LIMIT;
    while ((uint32_t)(adc_hw_count() - start) < startup_cycles) {
        if (--remaining == 0) {
            adc_hw_reset();
            return false;
        }
    }
    adc_initialized = true;
    return true;
}

bool uc3b_adc_read_channel(uint8_t channel, uint16_t *value)
{
    uint32_t sr, cpu_hz, pba_hz, timeout, start, remaining;
    bool success = false;
    if (channel >= UC3B_ADC_CHANNEL_COUNT || value == 0) { return false; }
    sr = adc_hw_irq_save();
    if (adc_busy) {
        adc_hw_irq_restore(sr);
        return false;
    }
    adc_busy = true;
    adc_hw_irq_restore(sr);
    cpu_hz = uc3b_clock_get_cpu_hz();
    pba_hz = uc3b_clock_get_pba_hz();
    /* Current clock HAL: RCSYS or OSC0, CPU=PBA. Allow divided buses too,
     * within UC3B's 60 MHz maximum. Reject implausible clock metadata.
     */
    if (pba_hz < 100000UL || cpu_hz < pba_hz || cpu_hz > 60000000UL) { goto done; }
    timeout = (cpu_hz + 99UL) / 100UL; /* 10 ms nominal, wrap-safe. */
    if (!adc_initialized && !adc_init(cpu_hz, pba_hz, timeout)) { goto done; }

    /* Idle here. Never change CHDR during a conversion (25.6.4). Exactly one
     * enabled channel also avoids the Rev B multi-channel ADC erratum.
     */
    adc_hw_disable_channels();
    (void)adc_hw_read(channel); /* Clear stale EOC before starting. */
    adc_hw_clear_last();       /* Clear DRDY to prevent an old overrun. */
    adc_hw_enable_channel(channel);
    adc_hw_start();
    start = adc_hw_count();
    remaining = ADC_POLL_LIMIT;
    while (!adc_hw_complete(channel)) {
        if (--remaining == 0 || (uint32_t)(adc_hw_count() - start) >= timeout) {
            /* Abort/reset BEFORE any channel change; next call reinitializes. */
            adc_hw_reset();
            adc_initialized = false;
            goto done;
        }
    }
    *value = adc_hw_read(channel);
    adc_hw_clear_last();
    adc_hw_disable_channels();
    success = true;
done:
    sr = adc_hw_irq_save();
    adc_busy = false;
    adc_hw_irq_restore(sr);
    return success;
}
