/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <Arduino.h>
#include "hal/uc3b_adc.h"

/* Test the actual variant with an unconfigured, analog, or digital D0. */
#define ADC_TEST_CONFIGURED (ADC_TEST_MODE != 0)
#define ADC_TEST_PORT (ADC_TEST_MODE == 2 ? 1 : 0)
#define ADC_TEST_BIT 3
#undef UC3BDUINO_TEST_PIN_CONFIGURED
#define UC3BDUINO_TEST_PIN_CONFIGURED ADC_TEST_CONFIGURED
#undef UC3BDUINO_TEST_PIN_PORT
#undef UC3BDUINO_TEST_PIN_BIT
#define UC3BDUINO_TEST_PIN_PORT ADC_TEST_PORT
#define UC3BDUINO_TEST_PIN_BIT ADC_TEST_BIT
#include "../arduino/hardware/uc3bduino/avr32/variants/uc3b0256/variant.cpp"

static Uc3bGpioPin selected;
static uint8_t selected_function, selected_channel;
static unsigned int mux_calls, read_calls, gpio_calls;
static bool fail_mux, fail_adc;
bool uc3b_gpio_is_valid(Uc3bGpioPin pin) { return (pin.port == 0 && pin.pin < 32) || (pin.port == 1 && pin.pin < 12); }
bool uc3b_gpio_select_peripheral(Uc3bGpioPin pin, uint8_t function)
{
    ++mux_calls; selected = pin; selected_function = function;
    return !fail_mux && uc3b_gpio_is_valid(pin);
}
bool uc3b_adc_read_channel(uint8_t channel, uint16_t *value)
{
    ++read_calls; selected_channel = channel; *value = 1023;
    return !fail_adc;
}
bool uc3b_gpio_config_input(Uc3bGpioPin pin) { selected = pin; ++gpio_calls; return true; }
bool uc3b_gpio_config_output(Uc3bGpioPin pin) { selected = pin; ++gpio_calls; return true; }
bool uc3b_gpio_enable_pullup(Uc3bGpioPin pin) { selected = pin; ++gpio_calls; return true; }
bool uc3b_gpio_write(Uc3bGpioPin pin, bool high) { (void)high; selected = pin; ++gpio_calls; return true; }
bool uc3b_gpio_read(Uc3bGpioPin pin) { selected = pin; ++gpio_calls; return true; }
#include "../arduino/hardware/uc3bduino/avr32/cores/uc3bduino/wiring_analog.cpp"
#include "../arduino/hardware/uc3bduino/avr32/cores/uc3bduino/wiring_digital.cpp"

int main(void)
{
    const uint8_t pads[] = { 3, 4, 5, 6, 7, 8, 30, 31 };
    const uint8_t functions[] = { 0, 0, 1, 1, 1, 1, 0, 0 };
    unsigned int i;
    assert(NUM_ANALOG_INPUTS == 8 && A0 == 1 && A7 == 8);
    assert(analogRead(NUM_DIGITAL_PINS) == -1 && analogRead(UINT32_MAX) == -1);
    assert(mux_calls == 0 && read_calls == 0);
    for (i = 0; i < 8; ++i) {
        assert(analogRead(A0 + i) == 1023);
        assert(selected.port == 0 && selected.pin == pads[i]);
        assert(selected_channel == i && selected_function == functions[i]);
        pinMode(A0 + i, INPUT); // same map also used for digital operations
        assert(selected.port == 0 && selected.pin == pads[i]);
    }
    assert(mux_calls == 8 && read_calls == 8 && gpio_calls == 8);
    if (ADC_TEST_CONFIGURED && ADC_TEST_PORT == 0 && ADC_TEST_BIT == 3) {
        assert(analogRead(D0) == 1023 && selected_channel == 0);
        assert(mux_calls == 9 && read_calls == 9);
    } else {
        assert(analogRead(D0) == -1 && mux_calls == 8 && read_calls == 8);
    }
    pinMode(D0, OUTPUT);
    assert(gpio_calls == (ADC_TEST_CONFIGURED ? 9u : 8u));
    if (ADC_TEST_CONFIGURED) {
        assert(selected.port == ADC_TEST_PORT && selected.pin == ADC_TEST_BIT);
    }
    fail_adc = true;
    assert(analogRead(A0) == -1);
    fail_mux = true; i = read_calls;
    assert(analogRead(A7) == -1 && read_calls == i);
    puts("PASS: analogRead API, all A-pins, D0 alias/non-ADC policy and digital mapping (fake HAL)");
    return 0;
}
