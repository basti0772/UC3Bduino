/* SPDX-License-Identifier: MIT */
#ifndef USB_FIFO_H
#define USB_FIFO_H
#include <stdint.h>

/* Walk byte addresses within the endpoint's virtual FIFO window. Repeating
 * byte accesses at one address corrupted SETUP data on the reference UC3B0256;
 * linear accesses pass real Windows enumeration. Restart at the window base
 * for each bank; the controller manages physical DPRAM bank allocation.
 */
static inline void usb_fifo_read_bytes(volatile const uint8_t *fifo,
                                      uint8_t *data, uint16_t size)
{
    while (size--) { *data++ = *fifo++; }
}

static inline void usb_fifo_write_bytes(volatile uint8_t *fifo,
                                       const uint8_t *data, uint16_t size)
{
    while (size--) { *fifo++ = *data++; }
}
#endif
