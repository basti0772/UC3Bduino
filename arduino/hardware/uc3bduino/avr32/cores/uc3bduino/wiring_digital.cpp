/* SPDX-License-Identifier: MIT */
#include <Arduino.h>
#include "hal/uc3b_gpio.h"

static bool mapped_pin(uint32_t number, Uc3bGpioPin *pin)
{
    if (number >= NUM_DIGITAL_PINS) {
        return false;
    }
    /* Copy fields explicitly: old freestanding GCC can emit memcpy for structs. */
    pin->port = uc3b_pin_map[number].port;
    pin->pin = uc3b_pin_map[number].pin;
    return uc3b_gpio_is_valid(*pin);
}

void pinMode(uint32_t number, uint32_t mode)
{
    Uc3bGpioPin pin;
    if (!mapped_pin(number, &pin)) {
        return;
    }
    switch (mode) {
    case INPUT:
        uc3b_gpio_config_input(pin);
        break;
    case OUTPUT:
        uc3b_gpio_config_output(pin);
        break;
    case INPUT_PULLUP:
        uc3b_gpio_config_input(pin);
        uc3b_gpio_enable_pullup(pin);
        break;
    default:
        break; /* Unsupported mode leaves the pin unchanged. */
    }
}

void digitalWrite(uint32_t number, uint32_t value)
{
    Uc3bGpioPin pin;
    if (mapped_pin(number, &pin)) {
        uc3b_gpio_write(pin, value != LOW);
    }
}

int digitalRead(uint32_t number)
{
    Uc3bGpioPin pin;
    return mapped_pin(number, &pin) && uc3b_gpio_read(pin) ? HIGH : LOW;
}
