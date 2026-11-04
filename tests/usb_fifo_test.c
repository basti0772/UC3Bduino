/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/usb/usb_fifo.h"

/* A byte-addressed window tests the production copy path and its bounds.
 * Hardware bank ownership and bus side effects still require a real board.
 */
int main(void)
{
    static const uint8_t setup[8] = { 0x80, 6, 0, 1, 0, 0, 64, 0 };
    static const uint16_t sizes[] = { 0, 1, 7, 8, 16, 63, 64 };
    volatile uint8_t window[66];
    uint8_t payload[64], result[66];
    unsigned i, test;
    for (i = 0; i < sizeof(payload); ++i) { payload[i] = (uint8_t)(i * 17 + 3); }
    memcpy(payload, setup, sizeof(setup));
    for (test = 0; test < sizeof(sizes) / sizeof(sizes[0]); ++test) {
        uint16_t size = sizes[test];
        for (i = 0; i < sizeof(window); ++i) { window[i] = 0xa5; }
        usb_fifo_write_bytes(window + 1, payload, size);
        assert(window[0] == 0xa5);
        for (i = 0; i < size; ++i) { assert(window[i + 1] == payload[i]); }
        for (i = size + 1; i < sizeof(window); ++i) { assert(window[i] == 0xa5); }
        memset(result, 0x5a, sizeof(result));
        usb_fifo_read_bytes(window + 1, result + 1, size);
        assert(result[0] == 0x5a);
        assert(memcmp(result + 1, payload, size) == 0);
        for (i = size + 1; i < sizeof(result); ++i) { assert(result[i] == 0x5a); }
    }
    puts("PASS: USB FIFO byte order and bounds, SETUP and 0..64-byte packets");
    return 0;
}
