/* SPDX-License-Identifier: MIT */
#ifndef UC3B_BOOTLOADER_HW_H
#define UC3B_BOOTLOADER_HW_H
#include "uc3b_device.h"
#include "uc3b_irq.h"
#include "uc3b_clock.h"

typedef char boot_layout_check[(AVR32_FLASHC_ADDRESS == 0xfffe1400UL
    && __builtin_offsetof(avr32_flashc_t, fcmd) == 4
    && __builtin_offsetof(avr32_flashc_t, fsr) == 8
    && __builtin_offsetof(avr32_flashc_t, fgpfrlo) == 16
    && AVR32_FLASHC_FCMD_CMD_EGPB == 8
    && AVR32_WDT_ADDRESS == 0xffff0d30UL
    && __builtin_offsetof(avr32_wdt_t, ctrl) == 0) ? 1 : -1];

#define boot_hw_lock uc3b_irq_save
#define boot_hw_unlock uc3b_irq_restore
static inline bool boot_hw_supported(void)
{
    uint32_t id, revision;
    __asm__ __volatile__("mfdr %0, 0" : "=r" (id)); /* OCD DID, read-only. */
    revision = id >> 28;
    /* 32059L 31.2: B has a different FGPFRLO address; B/F/G also require
     * RAM execution plus a dummy page write after fuse commands. This simple
     * backend must never issue a fuse command on those/unknown revisions.
     */
    return (id & 0x0fffffffUL) == 0x01ee403fUL
        && (revision == 8 || revision == 9 || revision == 10); /* I/J/K */
}
static inline bool boot_hw_ready(void)
{ return (AVR32_FLASHC.fsr & AVR32_FLASHC_FSR_FRDY_MASK) != 0; }
static inline uint32_t boot_hw_fuses(void) { return AVR32_FLASHC.fgpfrlo; }
static inline bool boot_hw_command_ok(void)
{
    return (AVR32_FLASHC.fsr & (AVR32_FLASHC_FSR_LOCKE_MASK
        | AVR32_FLASHC_FSR_PROGE_MASK)) == 0;
}
static inline void boot_hw_set_force(void)
{
    AVR32_FLASHC.fcmd = (0xa5UL << AVR32_FLASHC_FCMD_KEY_OFFSET)
        | (31UL << AVR32_FLASHC_FCMD_PAGEN_OFFSET) | AVR32_FLASHC_FCMD_CMD_EGPB;
}
static inline void boot_hw_reset(void)
{
    uint32_t start, control;
    (void)uc3b_irq_save();
    /* USB was detached by the caller. Keep it absent for at least 250 ms,
     * independent of the Arduino timer ISR, before arming the watchdog.
     */
    start = __builtin_mfsr(AVR32_COUNT);
    while ((uint32_t)(__builtin_mfsr(AVR32_COUNT) - start)
           < uc3b_clock_get_cpu_hz() / 4UL) { }
    /* 32059L ch.11: keyed writes; PSEL=15 is ~569 ms at nominal RCSYS.
     * WDT survives its own reset. The verified legacy ISP path disables it.
     * No branch to a bootloader address and no reset-cause fabrication.
     */
    control = (15UL << AVR32_WDT_CTRL_PSEL_OFFSET) | AVR32_WDT_CTRL_EN_MASK;
    AVR32_WDT.ctrl = control | (0x55UL << AVR32_WDT_CTRL_KEY_OFFSET);
    AVR32_WDT.ctrl = control | (0xaaUL << AVR32_WDT_CTRL_KEY_OFFSET);
    AVR32_WDT.clr = 0;
    for (;;) { __asm__ __volatile__("nop"); }
}
#endif
