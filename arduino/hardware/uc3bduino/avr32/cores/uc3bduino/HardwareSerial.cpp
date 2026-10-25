/* SPDX-License-Identifier: MIT */
#include "HardwareSerial.h"
#include "hal/uc3b_uart.h"
#include "serial_config.h"

HardwareSerial Serial; /* Trivial object: zeroed by startup, no constructor. */

static size_t serial_write_byte(uint8_t value)
{
    return uc3b_uart_write(value) ? 1 : 0;
}

void HardwareSerial::begin(unsigned long baud) { begin(baud, SERIAL_8N1); }

void HardwareSerial::begin(unsigned long baud, uint8_t config)
{
    Uc3bUartConfig uart;
    write_byte_ = 0;
    if (config != SERIAL_8N1) { uc3b_uart_end(); return; }
    uart.baudrate = baud;
    uart.route = UC3BDUINO_SERIAL_ROUTE;
    if (uc3b_uart_init(&uart)) { write_byte_ = serial_write_byte; }
}

void HardwareSerial::end() { uc3b_uart_end(); write_byte_ = 0; }
int HardwareSerial::available() { return uc3b_uart_available(); }
int HardwareSerial::read() { return uc3b_uart_read(); }
int HardwareSerial::peek() { return uc3b_uart_peek(); }
void HardwareSerial::flush() { uc3b_uart_flush(); }
HardwareSerial::operator bool() const { return uc3b_uart_active(); }
