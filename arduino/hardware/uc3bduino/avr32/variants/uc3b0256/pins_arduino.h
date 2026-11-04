/* SPDX-License-Identifier: MIT */
#pragma once
#include "hal/uc3b_gpio.h"
#include "test_pin_config.h"

#define NUM_DIGITAL_PINS 16
#define NUM_ANALOG_INPUTS 8
#define D0 0
#define A0 1
#define A1 2
#define A2 3
#define A3 4
#define A4 5
#define A5 6
#define A6 7
#define A7 8
#define LED_BUILTIN 9
#define LED_BUILTIN_ACTIVE_LOW 1
#define LED_BUILTIN_ON LOW
#define LED_BUILTIN_OFF HIGH
/* SPI route A, MCU pad names, not connector pin numbers. SS is software GPIO. */
#define MOSI 10
#define MISO 11
#define SCK 12
#define SS 13
#define SDA 14
#define SCL 15

typedef struct {
    uint8_t port, pin;
    int8_t adc_channel; /* -1: this pad has no ADC input. */
    uint8_t adc_function; /* Meaningful only with a valid adc_channel. */
} Uc3bPinDescription;

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
extern const Uc3bPinDescription uc3b_pin_map[NUM_DIGITAL_PINS];
#ifdef __cplusplus
}
#endif
