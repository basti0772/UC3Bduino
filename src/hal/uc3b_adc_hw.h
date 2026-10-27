/* SPDX-License-Identifier: MIT */
#ifndef UC3B_ADC_HW_H
#define UC3B_ADC_HW_H
#include "uc3b_device.h"
#include "uc3b_irq.h"

/* 32059L: clock map, table 4-1, chapter 25. Installed Atmel adc_200.h. */
typedef char uc3b_adc_layout_check[
    (AVR32_ADC_ADDRESS == 0xFFFF3C00UL && AVR32_ADC_CLK_PBA == 68
     && AVR32_ADC_CHANNELS_MSB == 7 && AVR32_ADC_DATA_MSB == 9
     && __builtin_offsetof(avr32_adc_t, cr) == 0x00
     && __builtin_offsetof(avr32_adc_t, mr) == 0x04
     && __builtin_offsetof(avr32_adc_t, cher) == 0x10
     && __builtin_offsetof(avr32_adc_t, chdr) == 0x14
     && __builtin_offsetof(avr32_adc_t, sr) == 0x1c
     && __builtin_offsetof(avr32_adc_t, lcdr) == 0x20
     && __builtin_offsetof(avr32_adc_t, idr) == 0x28
     && __builtin_offsetof(avr32_adc_t, cdr0) == 0x30
     && __builtin_offsetof(avr32_adc_t, cdr7) == 0x4c
     && AVR32_ADC_CDR0 == 0x30 && AVR32_ADC_CDR7 == 0x4c
     && AVR32_ADC_CDR0_DATA_MASK == 0x3ff
     && AVR32_ADC_SR_EOC0_MASK == 1 && AVR32_ADC_SR_EOC7_MASK == 0x80
     && AVR32_ADC_MR_PRESCAL_OFFSET == 8
     && AVR32_ADC_MR_STARTUP_OFFSET == 16
     && AVR32_ADC_MR_SHTIM_OFFSET == 24) ? 1 : -1];
typedef char uc3b_adc_pin_check[
    (AVR32_ADC_AD_0_PIN == 3 && AVR32_ADC_AD_0_FUNCTION == 0
     && AVR32_ADC_AD_1_PIN == 4 && AVR32_ADC_AD_1_FUNCTION == 0
     && AVR32_ADC_AD_2_PIN == 5 && AVR32_ADC_AD_2_FUNCTION == 1
     && AVR32_ADC_AD_3_PIN == 6 && AVR32_ADC_AD_3_FUNCTION == 1
     && AVR32_ADC_AD_4_PIN == 7 && AVR32_ADC_AD_4_FUNCTION == 1
     && AVR32_ADC_AD_5_PIN == 8 && AVR32_ADC_AD_5_FUNCTION == 1
     && AVR32_ADC_AD_6_PIN == 30 && AVR32_ADC_AD_6_FUNCTION == 0
     && AVR32_ADC_AD_7_PIN == 31 && AVR32_ADC_AD_7_FUNCTION == 0) ? 1 : -1];

#define adc_hw_irq_save uc3b_irq_save
#define adc_hw_irq_restore uc3b_irq_restore
static inline uint32_t adc_hw_count(void) { return __builtin_mfsr(AVR32_COUNT); }
static inline void adc_hw_enable_clock(void)
{
    uint32_t sr = uc3b_irq_save();
    AVR32_PM.pbamask |= 1UL << (AVR32_ADC_CLK_PBA % 32);
    uc3b_irq_restore(sr);
}
static inline bool adc_hw_clock_ready(void)
{
    return (AVR32_PM.poscsr & AVR32_PM_POSCSR_MSKRDY_MASK) != 0;
}
static inline void adc_hw_reset(void) { AVR32_ADC.cr = AVR32_ADC_CR_SWRST_MASK; }
static inline void adc_hw_configure(uint32_t prescaler)
{
    AVR32_ADC.idr = 0xffffffffUL; /* All ADC interrupts off; reserved bits ignored. */
    AVR32_ADC.mr = (prescaler << AVR32_ADC_MR_PRESCAL_OFFSET)
        | (15UL << AVR32_ADC_MR_STARTUP_OFFSET)
        | (15UL << AVR32_ADC_MR_SHTIM_OFFSET);
    /* LOWRES=SLEEP=TRGEN=0: 10 bits, normal mode, software trigger only. */
}
static inline void adc_hw_disable_channels(void) { AVR32_ADC.chdr = 0xffUL; }
static inline void adc_hw_enable_channel(uint8_t channel) { AVR32_ADC.cher = 1UL << channel; }
static inline void adc_hw_start(void) { AVR32_ADC.cr = AVR32_ADC_CR_START_MASK; }
static inline bool adc_hw_complete(uint8_t channel)
{
    return (AVR32_ADC.sr & (AVR32_ADC_SR_EOC0_MASK << channel)) != 0;
}
static inline uint16_t adc_hw_read(uint8_t channel)
{
    /* CDR0..7 are consecutive, read-only 32-bit MMIO words (25.7.11). */
    volatile const uint32_t *cdr = (volatile const uint32_t *)
        (AVR32_ADC_ADDRESS + AVR32_ADC_CDR0 + 4UL * channel);
    return (uint16_t)(*cdr & AVR32_ADC_CDR0_DATA_MASK);
}
static inline void adc_hw_clear_last(void) { (void)AVR32_ADC.lcdr; }
#endif
