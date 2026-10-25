/* SPDX-License-Identifier: MIT */
#ifndef UC3B_SYSTEM_TIMER_HW_H
#define UC3B_SYSTEM_TIMER_HW_H
#include "uc3b_device.h"
#include "uc3b_irq.h"

#define UC3B_SYSTEM_TIMER_ISR __attribute__((__interrupt__))

/* The installed headers are checked against 32059L chapters 12 and 23. */
typedef char uc3b_timer_layout_check[
    (AVR32_TC_ADDRESS == 0xFFFF3800UL && AVR32_TC_IRQ0 == 448
     && AVR32_TC_CLK_PBA == 76 && AVR32_INTC_CLK_PBA == 64
     && sizeof(avr32_tc_channel_t) == 0x40
     && __builtin_offsetof(avr32_tc_channel_t, ccr) == 0x00
     && __builtin_offsetof(avr32_tc_channel_t, cmr) == 0x04
     && __builtin_offsetof(avr32_tc_channel_t, cv) == 0x10
     && __builtin_offsetof(avr32_tc_channel_t, rc) == 0x1c
     && __builtin_offsetof(avr32_tc_channel_t, sr) == 0x20
     && __builtin_offsetof(avr32_tc_channel_t, ier) == 0x24
     && __builtin_offsetof(avr32_tc_channel_t, idr) == 0x28
     && AVR32_INTC_ADDRESS == 0xFFFF0800UL
     && __builtin_offsetof(avr32_intc_t, ipr) == 0
     && AVR32_TC_TCCLKS_TIMER_CLOCK3 == 2) ? 1 : -1];

#define timer_hw_irq_save uc3b_irq_save
#define timer_hw_irq_restore uc3b_irq_restore

static inline bool timer_hw_init_allowed(void)
{
    return (__builtin_mfsr(AVR32_SR) & AVR32_SR_GM_MASK) != 0;
}

static inline uint32_t timer_hw_counter(void)
{
    return AVR32_TC.channel[0].cv;
}

static inline bool timer_hw_take_period(void)
{
    /* SR is read-to-clear. Only the ISR or a GM-masked snapshot reads it. */
    return (AVR32_TC.channel[0].sr & AVR32_TC_CPCS_MASK) != 0;
}

static inline void timer_hw_ack_barrier(void)
{
    /* Read a NON-clearing register on the same peripheral after SR, then give
     * INTC its three CLK_SYNC propagation cycles before unmasking/rete.
     * Never dummy-read SR: that could discard the next compare event.
     */
    (void)AVR32_TC.channel[0].cv;
    __asm__ __volatile__("nop\n\tnop\n\tnop" : : : "memory");
}

static inline void timer_hw_configure(uint32_t ticks)
{
    extern const char __system_timer_vector_offset[]; /* Absolute linker symbol. */
    AVR32_PM.pbamask = AVR32_PM.pbamask
        | (1UL << (AVR32_TC_CLK_PBA % 32))
        | (1UL << (AVR32_INTC_CLK_PBA % 32));
    while ((AVR32_PM.poscsr & AVR32_PM_POSCSR_MSKRDY_MASK) == 0) {
    }
    AVR32_TC.channel[0].idr = 0xff; /* All eight channel interrupt sources. */
    AVR32_TC.channel[0].ccr = AVR32_TC_CLKDIS_MASK;
    (void)AVR32_TC.channel[0].sr;
    /* Capture mode, RC reset, no external trigger/pad mux or block SYNC.
     * TIMER_CLOCK3 = PBA/8 on both Rev B and later chips (32059L errata).
     */
    AVR32_TC.channel[0].cmr = AVR32_TC_CPCTRG_MASK
        | (AVR32_TC_TCCLKS_TIMER_CLOCK3 << AVR32_TC_TCCLKS_OFFSET);
    AVR32_TC.channel[0].rc = ticks;
    AVR32_INTC.ipr[AVR32_TC_IRQ0 / 32] =
        (uint32_t)__system_timer_vector_offset
        | (AVR32_INTC_INTLEVEL_INT0 << AVR32_INTC_INTLEVEL_OFFSET);
    AVR32_TC.channel[0].ier = AVR32_TC_CPCS_MASK;
    AVR32_TC.channel[0].ccr = AVR32_TC_CLKEN_MASK | AVR32_TC_SWTRG_MASK;
    timer_hw_ack_barrier();
}

static inline void timer_hw_enable(void)
{
    /* The single startup enable point, after state, vector and TC are ready. */
    __asm__ __volatile__("csrf %0\n\tcsrf %1"
                         : : "i" (AVR32_SR_I0M), "i" (AVR32_SR_GM) : "memory");
}
#endif
