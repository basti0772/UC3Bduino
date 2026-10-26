/* SPDX-License-Identifier: MIT */
#include "uc3b_uart.h"
#include "uc3b_clock.h"
#include "uc3b_uart_hw.h"

/* Volatile stores publish the byte before head; foreground accesses also use
 * a short GM critical section, never a wait with interrupts masked. */
static volatile uint8_t rx_buffer[SERIAL_RX_BUFFER_SIZE];
static volatile uint32_t rx_head, rx_tail;
static bool active;
#define RX_MASK (SERIAL_RX_BUFFER_SIZE - 1U)

bool uc3b_uart_baud_divider(uint32_t pba, uint32_t baud, uint32_t *divider)
{
    uint32_t denominator, cd, target, difference;
    if (!divider || pba == 0 || pba > 60000000UL || baud == 0 || baud > pba / 8) {
        return false;
    }
    denominator = baud * 8; /* <= PBA, so all intermediates fit in 32 bits. */
    cd = (pba + denominator / 2) / denominator;
    if (cd == 0 || cd > 65535) { return false; }
    target = denominator * cd;
    difference = pba > target ? pba - target : target - pba;
    if (difference > target / 50) { return false; } /* Relative error <= 2%. */
    *divider = cd;
    return true;
}

bool uc3b_uart_init(const Uc3bUartConfig *config)
{
    uint32_t cd, sr;
    uc3b_uart_end();
    if (!config || !uart_hw_route_valid(config->route)
        || !uc3b_uart_baud_divider(uc3b_clock_get_pba_hz(), config->baudrate, &cd)) {
        return false;
    }
    sr = uart_hw_irq_save();
    rx_head = rx_tail = 0;
    uart_hw_configure(config->route, cd);
    active = true;
    uart_hw_enable(); /* Clear I1M, but preserve caller's GM. */
    uart_hw_irq_restore(sr);
    return true;
}

void uc3b_uart_flush(void)
{
    if (active) {
        while ((uart_hw_status() & UC3B_UART_TX_EMPTY) == 0) { }
    }
}

void uc3b_uart_end(void)
{
    uint32_t sr;
    if (!active) { return; }
    uc3b_uart_flush(); /* Never mask interrupts while waiting for TX. */
    sr = uart_hw_irq_save();
    uart_hw_disable();
    active = false;
    rx_head = rx_tail = 0;
    uart_hw_irq_restore(sr);
}

bool uc3b_uart_active(void) { return active; }

int uc3b_uart_available(void)
{
    uint32_t sr = uart_hw_irq_save();
    int count = (int)((rx_head - rx_tail) & RX_MASK);
    uart_hw_irq_restore(sr);
    return count;
}

int uc3b_uart_peek(void)
{
    uint32_t sr = uart_hw_irq_save();
    int value = rx_head == rx_tail ? -1 : rx_buffer[rx_tail];
    uart_hw_irq_restore(sr);
    return value;
}

int uc3b_uart_read(void)
{
    uint32_t sr = uart_hw_irq_save();
    int value = -1;
    if (rx_head != rx_tail) {
        value = rx_buffer[rx_tail];
        rx_tail = (rx_tail + 1) & RX_MASK;
    }
    uart_hw_irq_restore(sr);
    return value;
}

bool uc3b_uart_write(uint8_t value)
{
    if (!active) { return false; }
    while ((uart_hw_status() & UC3B_UART_TX_READY) == 0) { }
    uart_hw_write(value);
    return true;
}

void UC3B_UART_ISR uc3b_uart_isr(void)
{
    uint32_t status = uart_hw_status();
    if (status & UC3B_UART_RX_READY) {
        uint8_t value = uart_hw_read(); /* Always drain RHR, even on overflow. */
        if ((status & UC3B_UART_ERRORS) == 0) {
            uint32_t next = (rx_head + 1) & RX_MASK;
            if (next != rx_tail) {
                rx_buffer[rx_head] = value;
                rx_head = next;
            } /* Full: discard newest, keep unread bytes in order. */
        }
    }
    if (status & UC3B_UART_ERRORS) { uart_hw_clear_errors(); }
    uart_hw_ack_barrier();
}
