/* SPDX-License-Identifier: MIT */
#include "uc3b_clock.h"
#include "uc3b_device.h"

void uc3b_clock_init(void)
{
    /* Datasheet 32059L sections 9.5.5/9.6: slow clock is always available
     * while the CPU runs. Preserve oscillator enables and flash wait states.
     */
    AVR32_PM.mcctrl = (AVR32_PM.mcctrl & ~AVR32_PM_MCCTRL_MCSEL_MASK)
        | (AVR32_PM_MCCTRL_MCSEL_SLOW << AVR32_PM_MCCTRL_MCSEL_OFFSET);

    /* Reset already gives CKSEL=0. Avoid a needless transition at reset;
     * never rewrite an in-flight divider change. All DIV and SEL fields
     * become zero together, so CPU=HSB=PBA=PBB=RCSYS.
     */
    if (AVR32_PM.cksel != 0) {
        while ((AVR32_PM.poscsr & AVR32_PM_POSCSR_CKRDY_MASK) == 0) {
        }
        AVR32_PM.cksel = 0;
        while ((AVR32_PM.poscsr & AVR32_PM_POSCSR_CKRDY_MASK) == 0) {
        }
    }

    /* Header clock indices span 32-bit domains: GPIO index 65 = PBA bit 1.
     * Use ordinary load/store RMW here, not AVR32 peripheral RMW opcodes.
     * No other module clocks are disabled. PM/PBA bridge must be accessible.
     */
    AVR32_PM.pbamask = AVR32_PM.pbamask
        | (1UL << (AVR32_GPIO_CLK_PBA % 32));
    while ((AVR32_PM.poscsr & AVR32_PM_POSCSR_MSKRDY_MASK) == 0) {
    }
}

uint32_t uc3b_clock_get_cpu_hz(void)
{
    return AVR32_PM_RCOSC_FREQUENCY;
}

uint32_t uc3b_clock_get_hsb_hz(void)
{
    return AVR32_PM_RCOSC_FREQUENCY;
}

uint32_t uc3b_clock_get_pba_hz(void)
{
    return AVR32_PM_RCOSC_FREQUENCY;
}

uint32_t uc3b_clock_get_pbb_hz(void)
{
    return AVR32_PM_RCOSC_FREQUENCY;
}
