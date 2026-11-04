/* SPDX-License-Identifier: MIT */
#include <Wire.h>
// Adapt address/register/count to a known device before connecting or uploading.
static const uint8_t DEVICE_ADDRESS = 0x50;
static const uint8_t REGISTER_ADDRESS = 0;
void setup() { SerialUSB.begin(); Wire.begin(); }
void loop()
{
    Wire.beginTransmission(DEVICE_ADDRESS);
    Wire.write(REGISTER_ADDRESS);
    uint8_t status = Wire.endTransmission(false); // Actually transmitted and ACKed, no STOP.
    if (!status && Wire.requestFrom(DEVICE_ADDRESS, 2) == 2) {
        if (SerialUSB) {
            SerialUSB.print("Register bytes (decimal): "); SerialUSB.print(Wire.read());
            SerialUSB.print(" "); SerialUSB.println(Wire.read());
        }
    } else {
        if (SerialUSB) { SerialUSB.print("I2C error: "); SerialUSB.println(status ? status : Wire.lastError()); }
        Wire.end(); Wire.begin(); // Bounded STOP/release and fresh configuration.
    }
    delay(1000);
}
