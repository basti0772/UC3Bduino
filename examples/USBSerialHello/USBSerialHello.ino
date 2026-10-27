/* SPDX-License-Identifier: MIT
 * Requires authorized VID/PID and verified OSC0: see docs/USB_CDC.md.
 */
void setup()
{
    SerialUSB.begin();
    uint32_t start = millis();
    while (!SerialUSB && (uint32_t)(millis() - start) < 3000UL) { }
}

void loop()
{
    if (SerialUSB) {
        SerialUSB.println("UC3Bduino USB CDC ready");
        SerialUSB.print("millis = ");
        SerialUSB.println(millis());
    }
    delay(1000); /* A late-opened Serial Monitor still sees the greeting. */
}
