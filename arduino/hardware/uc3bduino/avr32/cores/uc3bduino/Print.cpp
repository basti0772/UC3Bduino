/* SPDX-License-Identifier: MIT */
#include "Print.h"

typedef char uc3b_print_long_is_32_bits[(sizeof(unsigned long) == 4) ? 1 : -1];

size_t Print::write(uint8_t value)
{
    return write_byte_ ? write_byte_(value) : 0;
}

size_t Print::write(const uint8_t *buffer, size_t size)
{
    size_t count = 0;
    if (!buffer) { return 0; }
    while (count < size && write(buffer[count]) == 1) { ++count; }
    return count;
}

size_t Print::write(const char *text)
{
    size_t count = 0;
    if (!text) { return 0; }
    while (*text && write((uint8_t)*text) == 1) { ++text; ++count; }
    return count;
}

size_t Print::print(const char *text) { return write(text); }
size_t Print::print(char value) { return write((uint8_t)value); }
size_t Print::print(int value) { return print((long)value); }
size_t Print::print(unsigned int value) { return print((unsigned long)value); }

size_t Print::print(long value)
{
    if (value < 0) {
        if (write((uint8_t)'-') != 1) { return 0; }
        /* Unsigned subtraction also handles LONG_MIN without signed overflow. */
        return 1 + print(0UL - (unsigned long)value);
    }
    return print((unsigned long)value);
}

size_t Print::print(unsigned long value)
{
    uint8_t digits[10];
    size_t start = sizeof(digits);
    do {
        digits[--start] = (uint8_t)('0' + value % 10);
        value /= 10;
    } while (value);
    return write(digits + start, sizeof(digits) - start);
}

size_t Print::println() { return write("\r\n"); }
#define PRINTLN(type) size_t Print::println(type value) { size_t n = print(value); return n + println(); }
PRINTLN(const char *)
PRINTLN(char)
PRINTLN(int)
PRINTLN(unsigned int)
PRINTLN(long)
PRINTLN(unsigned long)
#undef PRINTLN
