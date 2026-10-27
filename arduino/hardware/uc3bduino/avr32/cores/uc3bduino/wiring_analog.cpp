/* SPDX-License-Identifier: MIT */
#include <Arduino.h>
#include "hal/uc3b_adc.h"

int analogRead(uint32_t number)
{
    Uc3bGpioPin pin;
    uint16_t value;
    if (number >= NUM_DIGITAL_PINS) { return -1; }
    const Uc3bPinDescription *description = &uc3b_pin_map[number];
    if (description->adc_channel < 0 || description->adc_channel >= UC3B_ADC_CHANNEL_COUNT) {
        return -1;
    }
    pin.port = description->port;
    pin.pin = description->pin;
    if (!uc3b_gpio_select_peripheral(pin, description->adc_function)) { return -1; }
    return uc3b_adc_read_channel((uint8_t)description->adc_channel, &value) ? (int)value : -1;
}
