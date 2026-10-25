/* SPDX-License-Identifier: MIT */
#pragma once
#include "hal/uc3b_gpio.h"
#include "test_pin_config.h"

#define NUM_DIGITAL_PINS 1
#define D0 0

/* Logical D0 is a configurable test slot, not a package/connector/LED number. */
#if UC3BDUINO_TEST_PIN_CONFIGURED
#if !((UC3BDUINO_TEST_PIN_PORT == 0 && UC3BDUINO_TEST_PIN_BIT >= 3 && UC3BDUINO_TEST_PIN_BIT < 32) \
   || (UC3BDUINO_TEST_PIN_PORT == 1 && UC3BDUINO_TEST_PIN_BIT >= 0 && UC3BDUINO_TEST_PIN_BIT < 12))
#error "Choose PA03..PA31 or PB00..PB11 after checking the board schematic"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif
extern const Uc3bGpioPin uc3b_pin_map[NUM_DIGITAL_PINS];
#ifdef __cplusplus
}
#endif
