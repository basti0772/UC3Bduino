/* SPDX-License-Identifier: MIT */
#include "USBSerial.h"
#include "usb/usb_cdc.h"

USBSerial SerialUSB; /* Trivial BSS object: no constructor or vtable. */
static size_t usb_write_byte(uint8_t value) { return usb_cdc_write(value) ? 1 : 0; }
void USBSerial::begin()
{
    write_byte_ = usb_cdc_begin() ? usb_write_byte : 0;
}
void USBSerial::begin(unsigned long baud) { (void)baud; begin(); }
void USBSerial::end() { usb_cdc_end(); write_byte_ = 0; }
int USBSerial::available() { return usb_cdc_available(); }
int USBSerial::read() { return usb_cdc_read(); }
int USBSerial::peek() { return usb_cdc_peek(); }
void USBSerial::flush() { usb_cdc_flush(); }
USBSerial::operator bool() const { return usb_cdc_ready(); }
