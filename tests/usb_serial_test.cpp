/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "USBSerial.h"
#include "usb/usb_cdc.h"
static bool started, connected;
static char captured[256];
static size_t used, capacity = sizeof(captured) - 1;
static unsigned flushes;
extern "C" {
bool usb_cdc_begin(void) { started = true; return true; }
void usb_cdc_end(void) { started = connected = false; }
bool usb_cdc_ready(void) { return started && connected; }
bool usb_cdc_write(uint8_t value)
{
    if (!usb_cdc_ready() || used == capacity) { return false; }
    captured[used++] = (char)value; captured[used] = 0; return true;
}
int usb_cdc_available(void) { return 1; }
int usb_cdc_read(void) { return 255; }
int usb_cdc_peek(void) { return 255; }
void usb_cdc_flush(void) { ++flushes; }
}
#include "../arduino/hardware/uc3bduino/avr32/cores/uc3bduino/Print.cpp"
#include "../arduino/hardware/uc3bduino/avr32/cores/uc3bduino/USBSerial.cpp"
int main(void)
{
    assert(!SerialUSB && SerialUSB.write('a') == 0);
    SerialUSB.begin(); assert(started && !SerialUSB && SerialUSB.println("lost") == 0);
    connected = true; assert(SerialUSB);
    assert(SerialUSB.print("Value: ") == 7 && SerialUSB.println(1234) == 6);
    assert(SerialUSB.println(4294967295UL) == 12);
    assert(strcmp(captured, "Value: 1234\r\n4294967295\r\n") == 0);
    assert(SerialUSB.peek() == 255 && SerialUSB.available() == 1 && SerialUSB.read() == 255);
    SerialUSB.flush(); assert(flushes == 1);
    capacity = used + 2; assert(SerialUSB.write("abcd") == 2);
    SerialUSB.end(); assert(!SerialUSB && SerialUSB.write('z') == 0);
    SerialUSB.begin(1200); assert(started); /* Compatibility only, no reset hook. */
    connected = true; used = 0; capacity = sizeof(captured) - 1;
    const uint8_t binary[] = {0, 128, 255};
    assert(SerialUSB.write(binary, sizeof(binary)) == 3 && memcmp(captured, binary, 3) == 0);
    SerialUSB.end();
    puts("PASS: SerialUSB begin overloads/Print/CRLF/short writes/binary/read/peek/flush/end");
    return 0;
}
