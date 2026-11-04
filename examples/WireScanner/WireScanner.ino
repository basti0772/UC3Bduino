/* SPDX-License-Identifier: MIT */
#include <Wire.h>
void setup()
{
    SerialUSB.begin();
    Wire.begin();
    Wire.setClock(100000UL);
}
void loop()
{
    unsigned int found = 0;
    for (uint8_t address = 8; address <= 0x77; ++address) {
        Wire.beginTransmission(address);
        uint8_t status = Wire.endTransmission(); // Address only; no dummy data write.
        if (status == 0) {
            ++found;
            if (SerialUSB) { SerialUSB.print("I2C address (decimal): "); SerialUSB.println(address); }
        } else if (status != 2) {
            if (SerialUSB) { SerialUSB.print("I2C bus/error: "); SerialUSB.println(status); }
            break; // Missing pull-ups or stuck bus is not 'no devices found'.
        }
    }
    if (SerialUSB) { SerialUSB.print("ACK count: "); SerialUSB.println(found); }
    delay(2000);
}
