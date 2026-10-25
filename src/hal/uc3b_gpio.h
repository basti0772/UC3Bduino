/* SPDX-License-Identifier: MIT */
#ifndef UC3B_GPIO_H
#define UC3B_GPIO_H

#include <stdbool.h>
#include <stdint.h>

enum { UC3B_GPIO_PORT_A = 0, UC3B_GPIO_PORT_B = 1 };

typedef struct {
    uint8_t port;
    uint8_t pin;
} Uc3bGpioPin;

/* AT32UC3B0256: PA00..PA31, PB00..PB11. Does not check board wiring,
 * JTAG/trace ownership or whether a pad is safe to drive.
 */
bool uc3b_gpio_is_valid(Uc3bGpioPin pin);

/* Call clock_init first. Mutators return false on invalid pins and perform
 * no register access. Configuration takes GPIO ownership, disables the
 * selected pin's interrupt and pull-up. Output starts LOW; input is floating.
 * Multi-register configuration is not atomic; serialize ownership changes.
 */
bool uc3b_gpio_config_output(Uc3bGpioPin pin);
bool uc3b_gpio_config_input(Uc3bGpioPin pin);
bool uc3b_gpio_enable_gpio(Uc3bGpioPin pin);
bool uc3b_gpio_write(Uc3bGpioPin pin, bool value);
bool uc3b_gpio_set(Uc3bGpioPin pin);
bool uc3b_gpio_clear(Uc3bGpioPin pin);
bool uc3b_gpio_toggle(Uc3bGpioPin pin);
bool uc3b_gpio_enable_pullup(Uc3bGpioPin pin);
bool uc3b_gpio_disable_pullup(Uc3bGpioPin pin);

/* Reads the sampled pad via PVR, not the output latch. Configure GPIO first.
 * Invalid pins return false without MMIO; is_valid distinguishes that case.
 */
bool uc3b_gpio_read(Uc3bGpioPin pin);

#endif
