/* SPDX-License-Identifier: MIT */
#ifndef UC3B_CLOCK_H
#define UC3B_CLOCK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UC3B_CLOCK_SOURCE_INTERNAL_RC 1
#define UC3B_CLOCK_SOURCE_OSC0 2
#ifndef UC3B_CLOCK_SOURCE
#define UC3B_CLOCK_SOURCE UC3B_CLOCK_SOURCE_INTERNAL_RC
#endif
#if UC3B_CLOCK_SOURCE == UC3B_CLOCK_SOURCE_OSC0
/* Explicit board inputs only. Mode 0 = external clock, 1 = crystal. */
#if !defined(UC3B_OSC0_HZ) || !defined(UC3B_OSC0_CRYSTAL)
#error "OSC0 requires verified UC3B_OSC0_HZ and UC3B_OSC0_CRYSTAL (0 or 1)"
#elif UC3B_OSC0_HZ < 400000UL || UC3B_OSC0_HZ > 16000000UL || (UC3B_OSC0_HZ % 800UL) != 0
#error "Supported OSC0 range: 0.4..16 MHz, divisible by 800 for the timebase"
#elif UC3B_OSC0_CRYSTAL != 0 && UC3B_OSC0_CRYSTAL != 1
#error "UC3B_OSC0_CRYSTAL must be 0 (external clock) or 1 (crystal)"
#endif
#elif UC3B_CLOCK_SOURCE != UC3B_CLOCK_SOURCE_INTERNAL_RC
#error "Unsupported clock source"
#endif

/* Call after hardware reset/startup, before GPIO, with interrupts masked.
 * Selects RCSYS, or explicitly configured OSC0, without division; enables GPIO.
 * Not a bootloader handoff or live peripheral clock-transition API.
 */
void uc3b_clock_init(void);

/* Nominal configured frequencies, NOT measurements. Valid after init while
 * this HAL owns clock configuration. RC calibration/temperature affect reality.
 */
uint32_t uc3b_clock_get_cpu_hz(void);
uint32_t uc3b_clock_get_hsb_hz(void);
uint32_t uc3b_clock_get_pba_hz(void);
uint32_t uc3b_clock_get_pbb_hz(void);

/* Own PLL1 and GCLK3 exclusively. OSC0 8/12/16 MHz only, within USB tolerance.
 * No CPU/bus divider changes. Call in foreground after clock_init().
 * Bounded failure leaves USB off. disable is safe after an unsuccessful enable.
 */
bool uc3b_clock_enable_usb(void);
void uc3b_clock_disable_usb(void);
uint32_t uc3b_clock_get_usb_hz(void);

#ifdef __cplusplus
}
#endif

#endif
