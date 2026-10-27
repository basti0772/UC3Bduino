/* SPDX-License-Identifier: MIT */
#ifndef UC3B_UART_H
#define UC3B_UART_H
#include <stdbool.h>
#include <stdint.h>

#ifndef SERIAL_RX_BUFFER_SIZE
#define SERIAL_RX_BUFFER_SIZE 64
#endif
#if SERIAL_RX_BUFFER_SIZE < 2 || SERIAL_RX_BUFFER_SIZE > 256 || (SERIAL_RX_BUFFER_SIZE & (SERIAL_RX_BUFFER_SIZE - 1)) != 0
#error "SERIAL_RX_BUFFER_SIZE must be a power of two in 2..256 (capacity size-1)"
#endif

#ifdef __cplusplus
extern "C" {
#endif
enum {
    UC3B_UART_U0A = 0, UC3B_UART_U0C, UC3B_UART_U1A,
    UC3B_UART_U1C, UC3B_UART_U2B, UC3B_UART_U2C,
    UC3B_UART_UNCONFIGURED = 255
};
typedef struct {
    uint32_t baudrate;
    uint8_t route;
} Uc3bUartConfig;

/* One UART, foreground calls only, after clock/timebase init. Reinit drains
 * TX and discards RX. Invalid route/rate leaves the UART inactive. Clock is
 * immutable until reset. RX requires interrupts and timely foreground reads.
 */
bool uc3b_uart_init(const Uc3bUartConfig *config);
void uc3b_uart_end(void);
bool uc3b_uart_active(void);
int uc3b_uart_available(void);
int uc3b_uart_read(void); /* -1 if empty, otherwise 0..255. */
int uc3b_uart_peek(void);
bool uc3b_uart_write(uint8_t value);
void uc3b_uart_flush(void); /* Wait for TXEMPTY; does not clear RX. */

/* Pure calculation; OVER=1, FP=0. Rejects >2% nominal error. Output untouched
 * on failure. Exposed internally to allow independent rate validation.
 */
bool uc3b_uart_baud_divider(uint32_t pba, uint32_t baud, uint32_t *divider);
#ifdef __cplusplus
}
#endif
#endif
