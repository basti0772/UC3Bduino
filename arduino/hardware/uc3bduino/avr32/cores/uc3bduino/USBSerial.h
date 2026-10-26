/* SPDX-License-Identifier: MIT */
#pragma once
#include "Print.h"

/* Shares the existing constructor-free Print sink ABI with HardwareSerial.
 * The current core has no Stream/timeout parser abstraction. One global object
 * is supported. All public methods are intended for setup()/loop().
 */
class USBSerial : public Print {
public:
    void begin();
    void begin(unsigned long baud); /* Compatibility argument, ignored. */
    void end();
    int available();
    int read();
    int peek();
    void flush(); /* Up to 100 ms; never discards RX. */
    operator bool() const; /* Configured, awake, and host DTR asserted. */
    /* Inherited write(byte/buffer/string) returns a short count on full TX. */
};
extern USBSerial SerialUSB;
