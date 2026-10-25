/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "HardwareSerial.h"
#include "hal/uc3b_uart.h"
static char output[512];
static size_t used, limit = sizeof(output) - 1;
static bool uart_active;
static unsigned flushes, endings;
static size_t capture(uint8_t byte)
{
    if (used == limit) { return 0; }
    output[used++] = (char)byte; output[used] = 0;
    return 1;
}
extern "C" {
bool uc3b_uart_init(const Uc3bUartConfig *config) { uart_active = config->baudrate == 115200; return uart_active; }
void uc3b_uart_end(void) { uart_active = false; ++endings; }
bool uc3b_uart_active(void) { return uart_active; }
bool uc3b_uart_write(uint8_t value) { return uart_active && capture(value) == 1; }
void uc3b_uart_flush(void) { ++flushes; }
int uc3b_uart_available(void) { return 1; }
int uc3b_uart_read(void) { return 255; }
int uc3b_uart_peek(void) { return 255; }
}
#include "../arduino/hardware/uc3bduino/avr32/cores/uc3bduino/Print.cpp"
#include "../arduino/hardware/uc3bduino/avr32/cores/uc3bduino/HardwareSerial.cpp"

int main(void)
{
    assert(!Serial && Serial.write('A') == 0 && Serial.println(123UL) == 0);
    Serial.begin(115200);
    assert(Serial);
    assert(Serial.println("UC3Bduino") == 11);
    assert(Serial.print('A') == 1);
    assert(Serial.println(0) == 3);
    assert(Serial.println(INT_MIN) == 13);
    assert(Serial.println(UINT_MAX) == 12);
    assert(Serial.println(LONG_MIN) == 13);
    assert(Serial.println(ULONG_MAX) == 12);
    assert(Serial.println(1L) == 3);
    assert(Serial.println('Z') == 3);
    assert(strcmp(output, "UC3Bduino\r\nA0\r\n-2147483648\r\n4294967295\r\n-2147483648\r\n4294967295\r\n1\r\nZ\r\n") == 0);
    assert(Serial.available() == 1 && Serial.peek() == 255 && Serial.read() == 255);
    Serial.flush(); assert(flushes == 1);
    used = 0; limit = 4;
    assert(Serial.print(123456UL) == 4 && strcmp(output, "1234") == 0);
    assert(Serial.write((const char *)0) == 0);
    assert(Serial.write((const uint8_t *)0, 4) == 0);
    Serial.end(); assert(!Serial && endings == 1 && Serial.print("x") == 0);
    Serial.begin(0); assert(!Serial && Serial.write('x') == 0);
    Serial.begin(115200, 0xff); assert(!Serial && endings == 2);
    used = 0; limit = sizeof(output) - 1;
    Serial.begin(115200, SERIAL_8N1);
    { const uint8_t binary[] = {0, 128, 255};
      assert(Serial.write(binary, sizeof(binary)) == 3);
      assert(memcmp(output, binary, sizeof(binary)) == 0); }
    puts("PASS: decimal Print limits/partial writes/CRLF, binary data and HardwareSerial API");
    return 0;
}
