/* SPDX-License-Identifier: MIT */
// Configure Serial pins and a suitable clock first; see docs/SERIAL.md.
void setup()
{
    Serial.begin(115200);
}

void loop()
{
    Serial.println(millis());
    delay(1000);
}
