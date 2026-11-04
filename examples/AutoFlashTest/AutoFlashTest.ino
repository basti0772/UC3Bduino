/* SPDX-License-Identifier: MIT */
/* Test-AutoUpload.ps1 prepends a unique ID to its private build copy. */
#ifndef UC3BDUINO_AUTOFLASH_BUILD_ID
#define UC3BDUINO_AUTOFLASH_BUILD_ID __DATE__ " " __TIME__
#endif

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
        SerialUSB.println("UC3Bduino AutoFlash Test");
        SerialUSB.print("Build: ");
        SerialUSB.println(UC3BDUINO_AUTOFLASH_BUILD_ID);
        SerialUSB.flush();
        SerialUSB.println("Board: AT32UC3B0256");
#if F_CPU == 12000000L
        SerialUSB.println("Clock: 12 MHz");
#else
        SerialUSB.print("Clock Hz: "); SerialUSB.println((unsigned long)F_CPU);
#endif
        SerialUSB.println("USB CDC: OK");
        SerialUSB.flush();
    }
    delay(500);
    digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF);
    delay(500);
}
