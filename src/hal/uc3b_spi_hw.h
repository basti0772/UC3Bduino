/* SPDX-License-Identifier: MIT */
#ifndef UC3B_SPI_HW_H
#define UC3B_SPI_HW_H
#include "uc3b_device.h"
#include "uc3b_gpio.h"
#include "uc3b_irq.h"

/* 32059L table 4-1, clock map, chapters 18 and 31.2; spi_199.h. */
typedef char spi_layout_check[(AVR32_SPI_ADDRESS == 0xffff2400UL
    && AVR32_SPI_CLK_PBA == 69
    && __builtin_offsetof(avr32_spi_t, cr) == 0
    && __builtin_offsetof(avr32_spi_t, mr) == 4
    && __builtin_offsetof(avr32_spi_t, rdr) == 8
    && __builtin_offsetof(avr32_spi_t, tdr) == 12
    && __builtin_offsetof(avr32_spi_t, sr) == 16
    && __builtin_offsetof(avr32_spi_t, idr) == 24
    && __builtin_offsetof(avr32_spi_t, csr0) == 48
    && AVR32_SPI_CSR0_SCBR_OFFSET == 8
    && AVR32_SPI_CSR0_NCPHA_MASK == 2 && AVR32_SPI_CSR0_CPOL_MASK == 1
    && AVR32_SPI_MOSI_0_0_PIN == 14 && AVR32_SPI_MOSI_0_0_FUNCTION == 0
    && AVR32_SPI_MISO_0_0_PIN == 25 && AVR32_SPI_MISO_0_0_FUNCTION == 0
    && AVR32_SPI_SCK_0_0_PIN == 15 && AVR32_SPI_SCK_0_0_FUNCTION == 0) ? 1 : -1];
#define SPI_HW_TDRE AVR32_SPI_SR_TDRE_MASK
#define SPI_HW_RDRF AVR32_SPI_SR_RDRF_MASK
#define SPI_HW_EMPTY AVR32_SPI_SR_TXEMPTY_MASK
#define SPI_HW_ENABLED AVR32_SPI_SR_SPIENS_MASK
#define SPI_HW_ERRORS (AVR32_SPI_SR_MODF_MASK | AVR32_SPI_SR_OVRES_MASK)
static inline bool spi_hw_foreground(void)
{
    uint32_t sr = __builtin_mfsr(AVR32_SR);
    return ((sr & AVR32_SR_M_MASK) >> AVR32_SR_M_OFFSET) == AVR32_SR_M_SUP;
}
static inline uint32_t spi_hw_count(void) { return __builtin_mfsr(AVR32_COUNT); }
static inline void spi_hw_clock_enable(void)
{
    uint32_t key = uc3b_irq_save();
    AVR32_PM.pbamask |= 1UL << (AVR32_SPI_CLK_PBA % 32);
    uc3b_irq_restore(key);
}
static inline bool spi_hw_clock_ready(void)
{ return (AVR32_PM.poscsr & AVR32_PM_POSCSR_MSKRDY_MASK) != 0; }
static inline void spi_hw_reset(void)
{
    AVR32_SPI.idr = 0xffffffffUL;
    AVR32_SPI.cr = AVR32_SPI_CR_SWRST_MASK;
}
static inline void spi_hw_mode(bool loopback)
{
    /* Fixed internal CS0 selects CSR0; NPCS pads stay GPIO. MODFDIS avoids
     * an external NSS influencing master operation. No PDCA or IRQ source.
     */
    AVR32_SPI.mr = AVR32_SPI_MR_MSTR_MASK | AVR32_SPI_MR_MODFDIS_MASK
        | (14UL << AVR32_SPI_MR_PCS_OFFSET)
        | (loopback ? AVR32_SPI_MR_LLB_MASK : 0);
}
static inline void spi_hw_configure(uint32_t csr) { AVR32_SPI.csr0 = csr; }
static inline void spi_hw_enable(void) { AVR32_SPI.cr = AVR32_SPI_CR_SPIEN_MASK; }
static inline uint32_t spi_hw_status(void) { return AVR32_SPI.sr; }
static inline void spi_hw_write(uint8_t value) { AVR32_SPI.tdr = value; }
static inline uint8_t spi_hw_read(void) { return (uint8_t)AVR32_SPI.rdr; }
static inline void spi_hw_pins(bool enable)
{
    const uint8_t pads[] = { 14, 25, 15 };
    unsigned int i;
    for (i = 0; i < sizeof(pads); ++i) {
        Uc3bGpioPin pin = { 0, pads[i] };
        if (enable) { (void)uc3b_gpio_select_peripheral(pin, 0); }
        else { (void)uc3b_gpio_config_input(pin); }
    }
}
#endif
