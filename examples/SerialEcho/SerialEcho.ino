/* SPDX-License-Identifier: MIT */
// Configure Serial pins and a suitable clock first; see docs/SERIAL.md.
void setup()
{
    Serial.begin(115200);
}

void loop()
{
    if (Serial.available() > 0) {
        int c = Serial.read();
        Serial.write((uint8_t)c);
    }
}
