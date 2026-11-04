/* SPDX-License-Identifier: MIT */
#include <Wire.h>
#ifndef UC3BDUINO_WIRE_BUILD_ID
#define UC3BDUINO_WIRE_BUILD_ID "local"
#endif
static unsigned int cases, failures;
static void check(bool condition) { ++cases; if (!condition) { ++failures; } }
void setup()
{
    SerialUSB.begin();
    pinMode(LED_BUILTIN, OUTPUT);
    Wire.begin(); // Releases SDA/SCL; no START, no external transaction.
    check((bool)Wire);
    Wire.setClock(400000UL); check(Wire.lastError() == 4);
    Wire.setClock(100000UL); check((bool)Wire);
    Wire.beginTransmission(0x78); check(Wire.endTransmission() == 4);
    Wire.beginTransmission(0x50);
    for (uint8_t i = 0; i < 32; ++i) { check(Wire.write(i) == 1); }
    check(Wire.write((uint8_t)32) == 0);
    check(Wire.endTransmission() == 1); // Overflow aborts before touching bus.
    check(Wire.requestFrom(0x50, 33) == 0);
    check(Wire.read() == -1 && Wire.peek() == -1 && Wire.available() == 0);
    Wire.setWireTimeout(0); check(Wire.lastError() == 4);
    Wire.setWireTimeout(); check(Wire.lastError() == 0);
    Wire.end(); check(!Wire);
}
void loop()
{
    if (SerialUSB) {
        SerialUSB.println("UC3Bduino Wire API smoke (no bus transfer)"); SerialUSB.flush();
        SerialUSB.print("Build: "); SerialUSB.println(UC3BDUINO_WIRE_BUILD_ID); SerialUSB.flush();
        SerialUSB.print("Cases: "); SerialUSB.println(cases);
        SerialUSB.print("Failures: "); SerialUSB.println(failures);
        SerialUSB.println(failures == 0 && cases == 43 ? "RESULT: PASS" : "RESULT: FAIL"); SerialUSB.flush();
    }
    digitalWrite(LED_BUILTIN, LED_BUILTIN_ON); delay(500);
    digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF); delay(500);
}
