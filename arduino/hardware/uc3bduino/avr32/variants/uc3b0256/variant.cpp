/* SPDX-License-Identifier: MIT */
#include "pins_arduino.h"

/* Table 4-1, 32059L: PA03/04 and PA30/31 use function A; PA05..08 B.
 * Derive metadata from the pad once, including a possible D0/A-pin alias.
 */
#define ADC_CHANNEL(p, b) ((p) != 0 ? -1 : ((b) >= 3 && (b) <= 8) ? (b) - 3 : \
    ((b) >= 30 && (b) <= 31) ? (b) - 24 : -1)
#define PIN(p, b) { (p), (b), ADC_CHANNEL(p, b), ((b) >= 5 && (b) <= 8) ? 1 : 0 }
const Uc3bPinDescription uc3b_pin_map[NUM_DIGITAL_PINS] = {
#if UC3BDUINO_TEST_PIN_CONFIGURED
    PIN(UC3BDUINO_TEST_PIN_PORT, UC3BDUINO_TEST_PIN_BIT),
#else
    PIN(255, 255), /* Unconfigured: HAL rejects it without touching registers. */
#endif
    PIN(0, 3), PIN(0, 4), PIN(0, 5), PIN(0, 6),
    PIN(0, 7), PIN(0, 8), PIN(0, 30), PIN(0, 31),
    PIN(0, 23), /* LED_BUILTIN: schematic D1, red, active low. */
    PIN(0, 14), PIN(0, 25), PIN(0, 15), PIN(0, 16), /* MOSI, MISO, SCK, SS */
    PIN(0, 10), PIN(0, 9) /* SDA, SCL: open-drain GPIO software master. */
};
#undef PIN
#undef ADC_CHANNEL
