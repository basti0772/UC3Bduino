/* SPDX-License-Identifier: MIT
 * Reference board red D1 is PA23, active low. Blinks even without USB/PC.
 */
void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF);
    SerialUSB.begin();
}
void loop()
{
    digitalWrite(LED_BUILTIN, LED_BUILTIN_ON);
    if (SerialUSB) {
        SerialUSB.println("UC3Bduino USB CDC ready");
        SerialUSB.print("UC3Bduino alive: ");
        SerialUSB.print(millis());
        SerialUSB.println(" ms");
    }
    delay(500);
    digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF);
    delay(500); /* Nominal 1 Hz period, including a small loop/ISR overhead. */
}
