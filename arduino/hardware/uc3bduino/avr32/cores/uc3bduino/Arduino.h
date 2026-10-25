/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>

#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

#ifdef __cplusplus
extern "C" {
#endif
void init(void);
void pinMode(uint32_t pin, uint32_t mode);
void digitalWrite(uint32_t pin, uint32_t value);
int digitalRead(uint32_t pin);
#ifdef __cplusplus
}
/* Sketch functions use C++ linkage; the startup entry main uses C linkage. */
void setup(void);
void loop(void);
#endif

#include "pins_arduino.h"
