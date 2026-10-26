/* SPDX-License-Identifier: MIT */
#pragma once
#include "Print.h"
#define SERIAL_8N1 0x06

/* Use the provided Serial object. No additional hardware instances are
 * supported. begin/end and I/O are foreground-only, not ISR/reentrant APIs.
 */
class HardwareSerial : public Print {
public:
    void begin(unsigned long baud);
    void begin(unsigned long baud, uint8_t config);
    void end();
    int available();
    int read();
    int peek();
    void flush();
    operator bool() const;
};
extern HardwareSerial Serial;
