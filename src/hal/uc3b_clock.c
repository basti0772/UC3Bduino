/* SPDX-License-Identifier: MIT */
#include "uc3b_clock.h"
#include "uc3b_device.h"

#if UC3B_CLOCK_SOURCE == UC3B_CLOCK_SOURCE_OSC0
#define UC3B_CONFIGURED_HZ UC3B_OSC0_HZ
#if !UC3B_OSC0_CRYSTAL
#define UC3B_OSC0_MODE AVR32_PM_OSCCTRL0_MODE_EXT_CLOCK
#elif UC3B_OSC0_HZ < 900000UL
#define UC3B_OSC0_MODE AVR32_PM_OSCCTRL0_MODE_CRYSTAL_G0
#elif UC3B_OSC0_HZ < 3000000UL
#define UC3B_OSC0_MODE AVR32_PM_OSCCTRL0_MODE_CRYSTAL_G1
#elif UC3B_OSC0_HZ < 8000000UL
#define UC3B_OSC0_MODE AVR32_PM_OSCCTRL0_MODE_CRYSTAL_G2
#else
#define UC3B_OSC0_MODE AVR32_PM_OSCCTRL0_MODE_CRYSTAL_G3
#endif
#else
#define UC3B_CONFIGURED_HZ AVR32_PM_RCOSC_FREQUENCY
#endif

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

#if UC3B_CLOCK_SOURCE == UC3B_CLOCK_SOURCE_OSC0
    /* Reset-only configuration, 32059L 9.5.2/9.6.5. OSC0 overrides PA18
     * (and PA19 in crystal mode). No GPIO mux writes on oscillator pads.
     * 16384 RCSYS cycles give nominally 142 ms startup. Missing/invalid OSC0
     * can stall startup; there is no fallback to a different UART clock.
     * <=16 MHz needs no extra flash wait state, no PLL is used.
     */
    AVR32_PM.mcctrl &= ~AVR32_PM_MCCTRL_OSC0EN_MASK;
    AVR32_PM.oscctrl0 = (UC3B_OSC0_MODE << AVR32_PM_OSCCTRL0_MODE_OFFSET)
        | (AVR32_PM_OSCCTRL0_STARTUP_16384_RCOSC << AVR32_PM_OSCCTRL0_STARTUP_OFFSET);
    AVR32_PM.mcctrl |= AVR32_PM_MCCTRL_OSC0EN_MASK;
    while ((AVR32_PM.poscsr & AVR32_PM_POSCSR_OSC0RDY_MASK) == 0) {
    }
    AVR32_PM.mcctrl = (AVR32_PM.mcctrl & ~AVR32_PM_MCCTRL_MCSEL_MASK)
        | (AVR32_PM_MCCTRL_MCSEL_OSC0 << AVR32_PM_MCCTRL_MCSEL_OFFSET);
#endif

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
    return UC3B_CONFIGURED_HZ;
}

uint32_t uc3b_clock_get_hsb_hz(void)
{
    return UC3B_CONFIGURED_HZ;
}

uint32_t uc3b_clock_get_pba_hz(void)
{
    return UC3B_CONFIGURED_HZ;
}

uint32_t uc3b_clock_get_pbb_hz(void)
{
    return UC3B_CONFIGURED_HZ;
}

static bool usb_clock_active;

void uc3b_clock_disable_usb(void)
{
    if (usb_clock_active) {
        AVR32_PM.gcctrl[AVR32_PM_GCLK_USBB] = 0;
        AVR32_PM.pll[1] = 0;
        usb_clock_active = false;
    }
}

bool uc3b_clock_enable_usb(void)
{
#if UC3B_CLOCK_SOURCE == UC3B_CLOCK_SOURCE_OSC0 && \
    (UC3B_OSC0_HZ == 8000000UL || UC3B_OSC0_HZ == 12000000UL || UC3B_OSC0_HZ == 16000000UL)
    uint32_t count;
    if (usb_clock_active) { return true; }
    /* Refuse to take PLL1/GCLK3 away from another owner. The reset-based
     * startup used here leaves them disabled; arbitrary handoff is unsupported.
     */
    if ((AVR32_PM.pll[1] & AVR32_PM_PLL1_PLLEN_MASK) != 0
        || (AVR32_PM.gcctrl[AVR32_PM_GCLK_USBB] & AVR32_PM_GCCTRL_CEN_MASK) != 0
        || (AVR32_PM.poscsr & AVR32_PM_POSCSR_OSC0RDY_MASK) == 0) { return false; }
    /* 32059L 9.5.4, 9.6.4, 28.7: DIV=0 -> FIN=OSC0 (4..16 MHz),
     * FVCO=2*(MUL+1)*OSC0=96 MHz. OPT[0]=low range, OPT[1]=divide by 2.
     * COUNT=63 slow-clock cycles is the maximum hardware lock qualification.
     */
    AVR32_PM.pll[1] = ((48000000UL / UC3B_OSC0_HZ - 1) << AVR32_PM_PLL1_PLLMUL_OFFSET)
        | (3UL << AVR32_PM_PLL1_PLLOPT_OFFSET) | AVR32_PM_PLL1_PLLCOUNT_MASK
        | AVR32_PM_PLL1_PLLEN_MASK;
    for (count = 0; count < 100000UL; ++count) {
        if (AVR32_PM.poscsr & AVR32_PM_POSCSR_LOCK1_MASK) { break; }
    }
    if (count == 100000UL) { AVR32_PM.pll[1] = 0; return false; }
    AVR32_PM.gcctrl[AVR32_PM_GCLK_USBB] = AVR32_PM_GCCTRL_PLLSEL_MASK
        | AVR32_PM_GCCTRL_OSCSEL_MASK | AVR32_PM_GCCTRL_CEN_MASK;
    usb_clock_active = true;
    return true;
#else
    return false; /* Uncalibrated RCSYS is never a USB clock source. */
#endif
}

uint32_t uc3b_clock_get_usb_hz(void)
{
    return usb_clock_active ? 48000000UL : 0;
}
