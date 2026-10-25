/* SPDX-License-Identifier: MIT */
#include "uc3b_gpio.h"
#include "uc3b_device.h"

bool uc3b_gpio_is_valid(Uc3bGpioPin pin)
{
    return (pin.port == UC3B_GPIO_PORT_A && pin.pin < 32)
        || (pin.port == UC3B_GPIO_PORT_B && pin.pin < 12);
}

bool uc3b_gpio_config_output(Uc3bGpioPin pin)
{
    volatile avr32_gpio_port_t *port;
    uint32_t mask;
    if (!uc3b_gpio_is_valid(pin)) {
        return false;
    }
    port = &AVR32_GPIO.port[pin.port];
    mask = 1UL << pin.pin;
    port->ierc = mask;
    port->oderc = mask;
    port->ovrc = mask; /* Preload LOW before enabling the driver. */
    port->puerc = mask;
    port->gpers = mask;
    port->oders = mask;
    return true;
}

bool uc3b_gpio_config_input(Uc3bGpioPin pin)
{
    volatile avr32_gpio_port_t *port;
    uint32_t mask;
    if (!uc3b_gpio_is_valid(pin)) {
        return false;
    }
    port = &AVR32_GPIO.port[pin.port];
    mask = 1UL << pin.pin;
    port->ierc = mask;
    port->oderc = mask;
    port->puerc = mask;
    port->gpers = mask;
    return true;
}

bool uc3b_gpio_enable_gpio(Uc3bGpioPin pin)
{
    if (!uc3b_gpio_is_valid(pin)) {
        return false;
    }
    AVR32_GPIO.port[pin.port].gpers = 1UL << pin.pin;
    return true;
}

bool uc3b_gpio_write(Uc3bGpioPin pin, bool value)
{
    return value ? uc3b_gpio_set(pin) : uc3b_gpio_clear(pin);
}

bool uc3b_gpio_set(Uc3bGpioPin pin)
{
    if (!uc3b_gpio_is_valid(pin)) {
        return false;
    }
    AVR32_GPIO.port[pin.port].ovrs = 1UL << pin.pin;
    return true;
}

bool uc3b_gpio_clear(Uc3bGpioPin pin)
{
    if (!uc3b_gpio_is_valid(pin)) {
        return false;
    }
    AVR32_GPIO.port[pin.port].ovrc = 1UL << pin.pin;
    return true;
}

bool uc3b_gpio_toggle(Uc3bGpioPin pin)
{
    if (!uc3b_gpio_is_valid(pin)) {
        return false;
    }
    AVR32_GPIO.port[pin.port].ovrt = 1UL << pin.pin;
    return true;
}

bool uc3b_gpio_enable_pullup(Uc3bGpioPin pin)
{
    if (!uc3b_gpio_is_valid(pin)) {
        return false;
    }
    AVR32_GPIO.port[pin.port].puers = 1UL << pin.pin;
    return true;
}

bool uc3b_gpio_disable_pullup(Uc3bGpioPin pin)
{
    if (!uc3b_gpio_is_valid(pin)) {
        return false;
    }
    AVR32_GPIO.port[pin.port].puerc = 1UL << pin.pin;
    return true;
}

bool uc3b_gpio_read(Uc3bGpioPin pin)
{
    if (!uc3b_gpio_is_valid(pin)) {
        return false;
    }
    return (AVR32_GPIO.port[pin.port].pvr & (1UL << pin.pin)) != 0;
}
