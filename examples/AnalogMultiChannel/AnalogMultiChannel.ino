/* SPDX-License-Identifier: MIT
 * A0=PA03, A1=PA04; sequential samples, not simultaneous. See docs/ADC.md.
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
}

void loop()
{
    int a0 = analogRead(A0);
    int a1 = analogRead(A1);
    ADC_PORT.print("A0=");
    ADC_PORT.print(a0);
    ADC_PORT.print(" A1=");
    ADC_PORT.println(a1); // -1 reports a failure; otherwise 0..1023.
    delay(100);
}
