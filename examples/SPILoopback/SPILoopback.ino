/* SPDX-License-Identifier: MIT */
#include <SPI.h>
#ifndef UC3BDUINO_SPI_BUILD_ID
#define UC3BDUINO_SPI_BUILD_ID "local"
#endif
static unsigned int cases, failures;
static bool finished;

static bool checkCase(uint32_t hz, uint8_t order, uint8_t mode)
{
    bool ok = true;
    uint8_t buffer[] = { 0, 1, 0x80, 0x96, 0x55, 0xaa, 0xff };
    const uint8_t expected[] = { 0, 1, 0x80, 0x96, 0x55, 0xaa, 0xff };
    SPI.beginTransaction(SPISettings(hz, order, mode));
    if (!SPI || !SPI.clockHz() || SPI.clockHz() > hz) { ok = false; }
    for (unsigned int i = 0; i < 256 && ok; ++i) {
        if (SPI.transfer((uint8_t)i) != i || SPI.lastError()) { ok = false; }
    }
    if (ok && SPI.transfer16(0x96e1) != 0x96e1) { ok = false; }
    if (ok) { SPI.transfer(buffer, sizeof(buffer)); }
    for (unsigned int i = 0; i < sizeof(buffer); ++i) {
        if (buffer[i] != expected[i]) { ok = false; }
    }
    if (SPI.lastError()) { ok = false; }
    SPI.endTransaction();
    return ok;
}
void setup()
{
    SerialUSB.begin();
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF);
    SPI.begin();
    if (!SPI.setLoopback(true)) { ++failures; }
}
void loop()
{
    if (!finished) {
        const uint32_t clocks[] = { 100000UL, 1000000UL, 4000000UL };
        for (unsigned int c = 0; c < 3; ++c) {
            for (uint8_t mode = 0; mode < 4; ++mode) {
                for (uint8_t order = 0; order < 2; ++order) {
                    ++cases;
                    if (!checkCase(clocks[c], order, (uint8_t)(mode << 2))) { ++failures; }
                }
            }
        }
        SPI.end();
        /* Repeat begin/end, then leave pins released while CDC stays available. */
        SPI.begin();
        if (!SPI.setLoopback(true) || !checkCase(1000000UL, MSBFIRST, SPI_MODE0)) { ++failures; }
        ++cases;
        SPI.end();
        finished = true;
    }
    if (SerialUSB) {
        SerialUSB.println("UC3Bduino SPI internal loopback"); SerialUSB.flush();
        SerialUSB.print("Build: "); SerialUSB.println(UC3BDUINO_SPI_BUILD_ID); SerialUSB.flush();
        SerialUSB.print("Cases: "); SerialUSB.println(cases);
        SerialUSB.print("Failures: "); SerialUSB.println(failures);
        SerialUSB.println(failures == 0 && cases == 25 ? "RESULT: PASS" : "RESULT: FAIL");
        SerialUSB.flush();
    }
    digitalWrite(LED_BUILTIN, LED_BUILTIN_ON); delay(500);
    digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF); delay(500);
}
