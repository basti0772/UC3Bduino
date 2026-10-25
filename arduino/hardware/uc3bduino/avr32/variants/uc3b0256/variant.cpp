/* SPDX-License-Identifier: MIT */
#include "pins_arduino.h"

const Uc3bGpioPin uc3b_pin_map[NUM_DIGITAL_PINS] = {
#if UC3BDUINO_TEST_PIN_CONFIGURED
    { UC3BDUINO_TEST_PIN_PORT, UC3BDUINO_TEST_PIN_BIT }
#else
    { 255, 255 } /* Unconfigured: HAL rejects it without touching registers. */
#endif
};
