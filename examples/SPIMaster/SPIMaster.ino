/* SPDX-License-Identifier: MIT */
#include <SPI.h>

void setup()
{
    SerialUSB.begin();
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF);
    pinMode(SS, OUTPUT);
    digitalWrite(SS, HIGH);
    SPI.begin();
}
void loop()
{
    SPI.beginTransaction(SPISettings(1000000UL, MSBFIRST, SPI_MODE0));
    if (SPI) {
        digitalWrite(SS, LOW);
        uint8_t response = SPI.transfer((uint8_t)0x55);
        digitalWrite(SS, HIGH);
        if (SerialUSB) {
            SerialUSB.print("SPI received (decimal): "); SerialUSB.println(response);
        }
    }
    SPI.endTransaction();
    if (SerialUSB && SPI.lastError()) {
        SerialUSB.print("SPI error: "); SerialUSB.println(SPI.lastError());
    }
    digitalWrite(LED_BUILTIN, LED_BUILTIN_ON); delay(500);
    digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF); delay(500);
}
