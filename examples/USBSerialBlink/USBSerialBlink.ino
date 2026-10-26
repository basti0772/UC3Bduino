/* SPDX-License-Identifier: MIT
 * D0 must be mapped explicitly to a safe, verified board pin by the installer.
 * No onboard LED or polarity is assumed. Pin toggles even without USB/PC.
 */
#if !UC3BDUINO_TEST_PIN_CONFIGURED
#error "Configure D0 from the board schematic before building USBSerialBlink"
#endif
void setup()
{
    pinMode(0, OUTPUT);
    SerialUSB.begin();
}
void loop()
{
    digitalWrite(0, HIGH);
    if (SerialUSB) {
        SerialUSB.println("UC3Bduino USB CDC ready");
        SerialUSB.print("UC3Bduino alive: ");
        SerialUSB.print(millis());
        SerialUSB.println(" ms");
    }
    delay(500);
    digitalWrite(0, LOW);
    delay(500); /* Nominal 1 Hz period, including a small loop/ISR overhead. */
}
