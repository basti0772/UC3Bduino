/* SPDX-License-Identifier: MIT */
#ifndef UC3B_SYSTEM_TIMER_H
#define UC3B_SYSTEM_TIMER_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reserved by UC3Bduino core for system timing: TC channel 0 and INTC group 14.
 * Call once after clock init, with GM set. Starts TC, then enables INT0 and GM.
 * False means unsupported clock or invalid initialization context; no start.
 * Live clock changes, sleep and arbitrary bootloader handoff are unsupported.
 */
bool uc3b_system_timer_init(void);
uint32_t uc3b_system_timer_millis(void);
uint32_t uc3b_system_timer_micros(void);

/* Service latency must remain below one 10 ms period, including the snapshot.
 * Reading time can service one pending period even with interrupts masked.
 * Do not call from NMI or a higher-priority ISR; see docs/TIMING.md.
 */
#ifdef __cplusplus
}
#endif
#endif
