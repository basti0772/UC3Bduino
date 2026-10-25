/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
#include <stdint.h>

/* Minimal decimal Print subset. A callback avoids a vtable/global constructor
 * on the reset-only core. Derived sinks must assign write_byte_ before use;
 * the sole supported Serial instance starts zero-initialized in .bss.
 * This is not the full virtual Arduino Print subclass ABI.
 */
class Print {
public:
    size_t write(uint8_t value);
    size_t write(const uint8_t *buffer, size_t size);
    size_t write(const char *text);
    size_t print(const char *text);
    size_t print(char value);
    size_t print(int value);
    size_t print(unsigned int value);
    size_t print(long value);
    size_t print(unsigned long value);
    size_t println();
    size_t println(const char *value);
    size_t println(char value);
    size_t println(int value);
    size_t println(unsigned int value);
    size_t println(long value);
    size_t println(unsigned long value);
protected:
    size_t (*write_byte_)(uint8_t);
};
