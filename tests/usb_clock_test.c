/* SPDX-License-Identifier: MIT
 * Real clock HAL against an ordinary volatile register fixture. Definitions
 * below are hardware facts from 32059L ch. 9, not copied vendor code/types.
 * Target builds separately check installed vendor offsets and bit definitions.
 */
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define UC3B_DEVICE_H
static volatile struct {
    uint32_t mcctrl, cksel, oscctrl0, poscsr, pbamask, pll[2], gcctrl[8];
} pm_fixture;
#define AVR32_PM pm_fixture
#define AVR32_PM_RCOSC_FREQUENCY 115200UL
#define AVR32_GPIO_CLK_PBA 65
#define AVR32_PM_GCLK_USBB 3
#define AVR32_PM_MCCTRL_MCSEL_MASK 3UL
#define AVR32_PM_MCCTRL_MCSEL_OFFSET 0
#define AVR32_PM_MCCTRL_MCSEL_SLOW 0
#define AVR32_PM_MCCTRL_MCSEL_OSC0 1
#define AVR32_PM_MCCTRL_OSC0EN_MASK 4UL
#define AVR32_PM_OSCCTRL0_MODE_EXT_CLOCK 0
#define AVR32_PM_OSCCTRL0_MODE_CRYSTAL_G0 4
#define AVR32_PM_OSCCTRL0_MODE_CRYSTAL_G1 5
#define AVR32_PM_OSCCTRL0_MODE_CRYSTAL_G2 6
#define AVR32_PM_OSCCTRL0_MODE_CRYSTAL_G3 7
#define AVR32_PM_OSCCTRL0_MODE_OFFSET 0
#define AVR32_PM_OSCCTRL0_STARTUP_16384_RCOSC 6
#define AVR32_PM_OSCCTRL0_STARTUP_OFFSET 8
#define AVR32_PM_POSCSR_CKRDY_MASK 32UL
#define AVR32_PM_POSCSR_MSKRDY_MASK 64UL
#define AVR32_PM_POSCSR_OSC0RDY_MASK 128UL
#define AVR32_PM_POSCSR_LOCK1_MASK 2UL
#define AVR32_PM_PLL1_PLLEN_MASK 1UL
#define AVR32_PM_PLL1_PLLMUL_OFFSET 16
#define AVR32_PM_PLL1_PLLOPT_OFFSET 2
#define AVR32_PM_PLL1_PLLCOUNT_MASK 0x3f000000UL
#define AVR32_PM_GCCTRL_OSCSEL_MASK 1UL
#define AVR32_PM_GCCTRL_PLLSEL_MASK 2UL
#define AVR32_PM_GCCTRL_CEN_MASK 4UL
#include "../src/hal/uc3b_clock.c"

int main(void)
{
    pm_fixture.mcctrl = 5; pm_fixture.cksel = 0x1234;
    pm_fixture.pll[0] = 0x55667788; pm_fixture.gcctrl[2] = 0x11223344;
    pm_fixture.poscsr = AVR32_PM_POSCSR_OSC0RDY_MASK | AVR32_PM_POSCSR_LOCK1_MASK;
#if UC3B_CLOCK_SOURCE == UC3B_CLOCK_SOURCE_OSC0 && \
    (UC3B_OSC0_HZ == 8000000UL || UC3B_OSC0_HZ == 12000000UL || UC3B_OSC0_HZ == 16000000UL)
    assert(uc3b_clock_enable_usb() && uc3b_clock_get_usb_hz() == 48000000UL);
    assert(pm_fixture.gcctrl[3] == 7);
    assert(pm_fixture.pll[1] == (0x3f00000dUL | ((48000000UL / UC3B_OSC0_HZ - 1) << 16)));
    assert(uc3b_clock_enable_usb()); /* idempotent */
    uc3b_clock_disable_usb(); assert(pm_fixture.pll[1] == 0 && pm_fixture.gcctrl[3] == 0);
    assert(uc3b_clock_get_usb_hz() == 0);
    pm_fixture.poscsr = AVR32_PM_POSCSR_OSC0RDY_MASK;
    assert(!uc3b_clock_enable_usb() && pm_fixture.pll[1] == 0 && pm_fixture.gcctrl[3] == 0); /* lock timeout */
    pm_fixture.poscsr = AVR32_PM_POSCSR_LOCK1_MASK;
    assert(!uc3b_clock_enable_usb()); /* missing OSC0 ready */
    pm_fixture.poscsr |= AVR32_PM_POSCSR_OSC0RDY_MASK;
    pm_fixture.pll[1] = 1; assert(!uc3b_clock_enable_usb());
    uc3b_clock_disable_usb(); assert(pm_fixture.pll[1] == 1); /* preserve foreign owner */
    pm_fixture.pll[1] = 0; pm_fixture.gcctrl[3] = 4;
    assert(!uc3b_clock_enable_usb()); uc3b_clock_disable_usb(); assert(pm_fixture.gcctrl[3] == 4);
#else
    assert(!uc3b_clock_enable_usb() && uc3b_clock_get_usb_hz() == 0);
    assert(pm_fixture.pll[1] == 0 && pm_fixture.gcctrl[3] == 0);
#endif
    assert(pm_fixture.mcctrl == 5 && pm_fixture.cksel == 0x1234);
    assert(pm_fixture.pll[0] == 0x55667788 && pm_fixture.gcctrl[2] == 0x11223344);
    assert(uc3b_clock_get_cpu_hz() == uc3b_clock_get_pba_hz()
        && uc3b_clock_get_cpu_hz() == uc3b_clock_get_pbb_hz());
    puts("PASS: USB PLL/GCLK configuration, clock preservation, ownership, bounded failures");
    return 0;
}
