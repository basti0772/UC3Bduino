/* SPDX-License-Identifier: MIT */
#ifndef UC3B_ADC_H
#define UC3B_ADC_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UC3B_ADC_CHANNEL_COUNT 8
#define UC3B_ADC_MAX_VALUE 1023

/* Foreground, exclusive ADC owner. Caller first selects the pad's ADC function
 * through the GPIO HAL. Lazy initialization; one software-triggered 10-bit
 * conversion. False on invalid argument, busy, unsupported clock or timeout;
 * *value stays unchanged on failure. No IRQ, DMA or timer ownership.
 */
bool uc3b_adc_read_channel(uint8_t channel, uint16_t *value);

#ifdef __cplusplus
}
#endif
#endif
