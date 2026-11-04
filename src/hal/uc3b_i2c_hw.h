/* SPDX-License-Identifier: MIT */
#ifndef UC3B_I2C_HW_H
#define UC3B_I2C_HW_H
#include "uc3b_device.h"
/* PA09/SCL, PA10/SDA. Use GPIO to support real address-only probes and general
 * repeated STARTs, without the legacy TWI block's automatic-STOP limitation.
 * Open drain: latch stays LOW; output-enable drives LOW, disable releases.
 */
#define I2C_SCL_MASK (1UL << 9)
#define I2C_SDA_MASK (1UL << 10)
typedef char i2c_gpio_layout_check[(AVR32_GPIO_ADDRESS == 0xffff1000UL
    && __builtin_offsetof(avr32_gpio_port_t, oders) == 0x44
    && __builtin_offsetof(avr32_gpio_port_t, oderc) == 0x48
    && __builtin_offsetof(avr32_gpio_port_t, ovrc) == 0x58
    && __builtin_offsetof(avr32_gpio_port_t, pvr) == 0x60
    && AVR32_TWI_SCL_0_0_PIN == 9 && AVR32_TWI_SDA_0_0_PIN == 10) ? 1 : -1];
static inline bool i2c_hw_foreground(void)
{
    uint32_t sr = __builtin_mfsr(AVR32_SR);
    return ((sr & AVR32_SR_M_MASK) >> AVR32_SR_M_OFFSET) == AVR32_SR_M_SUP;
}
static inline uint32_t i2c_hw_count(void) { return __builtin_mfsr(AVR32_COUNT); }
static inline void i2c_hw_scl(bool release)
{
    if (release) { AVR32_GPIO.port[0].oderc = I2C_SCL_MASK; }
    else { AVR32_GPIO.port[0].oders = I2C_SCL_MASK; }
}
static inline void i2c_hw_sda(bool release)
{
    if (release) { AVR32_GPIO.port[0].oderc = I2C_SDA_MASK; }
    else { AVR32_GPIO.port[0].oders = I2C_SDA_MASK; }
}
static inline bool i2c_hw_read_scl(void) { return (AVR32_GPIO.port[0].pvr & I2C_SCL_MASK) != 0; }
static inline bool i2c_hw_read_sda(void) { return (AVR32_GPIO.port[0].pvr & I2C_SDA_MASK) != 0; }
static inline void i2c_hw_init(void)
{
    uint32_t mask = I2C_SCL_MASK | I2C_SDA_MASK;
    AVR32_GPIO.port[0].oderc = mask;
    AVR32_GPIO.port[0].ovrc = mask;
    AVR32_GPIO.port[0].ierc = mask;
    AVR32_GPIO.port[0].puerc = mask; /* External pull-ups required. */
    AVR32_GPIO.port[0].gpers = mask;
}
#endif
