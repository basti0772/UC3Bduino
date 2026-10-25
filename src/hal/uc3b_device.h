/* SPDX-License-Identifier: MIT */
#ifndef UC3B_DEVICE_H
#define UC3B_DEVICE_H

#if !defined(__AVR32_UC3B0256__)
#error "UC3Bduino HAL currently supports only -mpart=uc3b0256"
#endif

/* Installed Atmel headers; provenance and notices: docs/THIRD_PARTY.md. */
#include <avr32/uc3b0256.h>

/* C99 compile-time checks of the official ABI against datasheet 32059L.
 * Fail rather than silently accessing the wrong offsets with another toolset.
 */
typedef char uc3b_pm_layout_check[
    (sizeof(unsigned long) == 4
     && AVR32_PM_ADDRESS == 0xFFFF0C00UL
     && __builtin_offsetof(avr32_pm_t, mcctrl) == 0x00
     && __builtin_offsetof(avr32_pm_t, cksel) == 0x04
     && __builtin_offsetof(avr32_pm_t, pbamask) == 0x10
     && __builtin_offsetof(avr32_pm_t, poscsr) == 0x54) ? 1 : -1];
typedef char uc3b_gpio_layout_check[
    (AVR32_GPIO_ADDRESS == 0xFFFF1000UL
     && AVR32_GPIO_PORT_LENGTH == 2
     && AVR32_GPIO_NUMBER_OF_PINS == 44
     && sizeof(avr32_gpio_port_t) == 0x100
     && __builtin_offsetof(avr32_gpio_port_t, gpers) == 0x04
     && __builtin_offsetof(avr32_gpio_port_t, oders) == 0x44
     && __builtin_offsetof(avr32_gpio_port_t, oderc) == 0x48
     && __builtin_offsetof(avr32_gpio_port_t, ovrs) == 0x54
     && __builtin_offsetof(avr32_gpio_port_t, ovrc) == 0x58
     && __builtin_offsetof(avr32_gpio_port_t, ovrt) == 0x5C
     && __builtin_offsetof(avr32_gpio_port_t, pvr) == 0x60
     && __builtin_offsetof(avr32_gpio_port_t, puers) == 0x74
     && __builtin_offsetof(avr32_gpio_port_t, puerc) == 0x78
     && __builtin_offsetof(avr32_gpio_port_t, ierc) == 0x98) ? 1 : -1];

#endif
