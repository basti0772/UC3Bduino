/* SPDX-License-Identifier: MIT */
#ifndef UC3B_CLOCK_H
#define UC3B_CLOCK_H

#include <stdint.h>

#define UC3B_CLOCK_SOURCE_INTERNAL_RC 1
#ifndef UC3B_CLOCK_SOURCE
#define UC3B_CLOCK_SOURCE UC3B_CLOCK_SOURCE_INTERNAL_RC
#endif
#if UC3B_CLOCK_SOURCE != UC3B_CLOCK_SOURCE_INTERNAL_RC
#error "Only the internal RC clock is implemented; verify board clocks before adding another source"
#endif

/* Call after hardware reset/startup, before GPIO, with interrupts masked.
 * Selects RCSYS without division and enables the GPIO PBA clock.
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

#endif
