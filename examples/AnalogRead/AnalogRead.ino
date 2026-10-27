/* SPDX-License-Identifier: MIT
 * Verify ADVREF, VDDANA, GNDANA and A0=PA03 wiring first; see docs/ADC.md.
 * Set ADC_USE_USB to 0 for the configured HardwareSerial route at 115200 baud.
 */
#ifndef ADC_USE_USB
#define ADC_USE_USB 1
#endif
#if ADC_USE_USB
#define ADC_PORT SerialUSB
#else
#define ADC_PORT Serial
#endif

void setup()
{
    ADC_PORT.begin(115200);
    // Do not wait for a USB host: sampling continues while disconnected.
}

void loop()
{
    int value = analogRead(A0);
    if (value < 0) {
        ADC_PORT.println("ADC error");
    } else {
        ADC_PORT.println(value); // Native 0..1023; no assumed reference voltage.
    }
    delay(100);
}
