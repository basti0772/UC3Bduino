/* SPDX-License-Identifier: MIT */
#ifndef UC3B_IRQ_H
#define UC3B_IRQ_H
#include "uc3b_device.h"
#include <stdint.h>

static inline uint32_t uc3b_irq_save(void)
{
    uint32_t sr;
    /* Keep save/mask together; two NOPs honor the Rev B masking erratum. */
    __asm__ __volatile__("mfsr %0, %1\n\tssrf %2\n\tnop\n\tnop"
                         : "=r" (sr) : "i" (AVR32_SR), "i" (AVR32_SR_GM)
                         : "memory");
    return sr;
}

static inline void uc3b_irq_restore(uint32_t sr)
{
    /* Restore only GM, not SR mode or arithmetic condition flags. */
    if ((sr & AVR32_SR_GM_MASK) == 0) {
        __asm__ __volatile__("csrf %0" : : "i" (AVR32_SR_GM) : "memory");
    }
    __asm__ __volatile__("" : : : "memory");
}
#endif
