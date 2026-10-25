/* SPDX-License-Identifier: MIT
 * Real UART/ring logic, deterministic hardware adapter; not a silicon emulator.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define UC3B_UART_HW_H
#define UC3B_UART_ISR
#define UC3B_UART_RX_READY 1U
#define UC3B_UART_TX_READY 2U
#define UC3B_UART_TX_EMPTY 512U
#define UC3B_UART_ERRORS (32U | 64U | 128U)
static uint32_t fake_pba = 12000000, gm, fake_status, configured_divider, clear_count, configured;
static uint32_t status_reads, ready_after, empty_after;
static uint8_t received, sent;
static bool enabled;
uint32_t uc3b_clock_get_pba_hz(void) { return fake_pba; }
static uint32_t uart_hw_irq_save(void) { uint32_t old = gm; gm = 1; return old; }
static void uart_hw_irq_restore(uint32_t old) { gm = old; }
static bool uart_hw_route_valid(uint8_t route) { return route < 6; }
static void uart_hw_configure(uint8_t route, uint32_t cd)
{
    assert(gm && route < 6); configured_divider = cd; ++configured;
}
static void uart_hw_enable(void) { assert(gm); enabled = true; }
static void uart_hw_disable(void) { assert(gm); enabled = false; }
static uint32_t uart_hw_status(void)
{
    ++status_reads;
    if (status_reads >= ready_after) { fake_status |= UC3B_UART_TX_READY; }
    if (status_reads >= empty_after) { fake_status |= UC3B_UART_TX_EMPTY; }
    return fake_status;
}
static uint8_t uart_hw_read(void) { fake_status &= ~UC3B_UART_RX_READY; return received; }
static void uart_hw_write(uint8_t value) { assert(fake_status & UC3B_UART_TX_READY); sent = value; }
static void uart_hw_clear_errors(void) { ++clear_count; fake_status &= ~UC3B_UART_ERRORS; }
static void uart_hw_ack_barrier(void) { }
#include "../src/hal/uc3b_uart.c"

static void receive(uint8_t value, uint32_t errors)
{
    assert(enabled);
    received = value; fake_status |= UC3B_UART_RX_READY | errors;
    gm = 1; uc3b_uart_isr(); gm = 0;
    assert((fake_status & (UC3B_UART_RX_READY | UC3B_UART_ERRORS)) == 0);
}

int main(void)
{
    Uc3bUartConfig config = {115200, UC3B_UART_U0C};
    const uint32_t rates[] = {9600, 19200, 38400, 57600, 115200};
    const uint32_t expected[] = {156, 78, 39, 26, 13};
    uint32_t cd, i, round, rng = 123, count = 0;
    uint8_t reference[SERIAL_RX_BUFFER_SIZE];
    assert(!uc3b_uart_active() && !uc3b_uart_write(1));
    assert(uc3b_uart_read() == -1 && uc3b_uart_peek() == -1);
    uc3b_uart_flush(); uc3b_uart_end();
    assert(status_reads == 0 && configured == 0);
    assert(!uc3b_uart_init(0));
    cd = 123;
    assert(!uc3b_uart_baud_divider(0, 9600, &cd) && cd == 123);
    assert(!uc3b_uart_baud_divider(UINT32_MAX, 9600, &cd));
    assert(!uc3b_uart_baud_divider(fake_pba, UINT32_MAX, &cd));
    assert(!uc3b_uart_baud_divider(fake_pba, 0, &cd));
    assert(!uc3b_uart_baud_divider(fake_pba, 1, &cd));
    assert(!uc3b_uart_baud_divider(fake_pba, 9600, 0));
    for (i = 0; i < 5; ++i) {
        assert(uc3b_uart_baud_divider(fake_pba, rates[i], &cd) && cd == expected[i]);
        assert(!uc3b_uart_baud_divider(115200, rates[i], &cd));
    }
    assert(uc3b_uart_baud_divider(115200, 14400, &cd) && cd == 1);
    assert(uc3b_uart_baud_divider(524280, 1, &cd) && cd == 65535);
    assert(!uc3b_uart_baud_divider(524288, 1, &cd));
    assert(uc3b_uart_baud_divider(81600, 10000, &cd) && cd == 1); /* Exactly +2%. */
    assert(!uc3b_uart_baud_divider(81601, 10000, &cd));
    assert(uc3b_uart_baud_divider(78400, 5000, &cd) && cd == 2); /* Exactly -2%. */
    assert(!uc3b_uart_baud_divider(78399, 5000, &cd));
    /* Independent rational oracle: accepted rates must satisfy the true error
     * inequality, without rounding actual baud to an integer. */
    for (i = 1; i < 2000000; i += 127) {
        if (uc3b_uart_baud_divider(fake_pba, i, &cd)) {
            uint64_t target = (uint64_t)i * 8 * cd;
            uint64_t delta = fake_pba > target ? fake_pba - target : target - fake_pba;
            assert(cd >= 1 && cd <= 65535 && delta * 50 <= target);
        }
    }
    assert(uc3b_uart_init(&config) && configured_divider == 13 && !gm && enabled);
    assert(uc3b_uart_available() == 0);
    for (i = 0; i < SERIAL_RX_BUFFER_SIZE + 4; ++i) { receive((uint8_t)i, 0); }
    assert(uc3b_uart_available() == SERIAL_RX_BUFFER_SIZE - 1);
    for (i = 0; i < SERIAL_RX_BUFFER_SIZE - 1; ++i) {
        gm = 1;
        assert(uc3b_uart_peek() == (int)(uint8_t)i && gm);
        assert(uc3b_uart_available() == (int)(SERIAL_RX_BUFFER_SIZE - 1 - i) && gm);
        assert(uc3b_uart_read() == (int)(uint8_t)i && gm);
        gm = 0;
    }
    for (i = 32; i <= 128; i *= 2) {
        receive(0xaa, i);
        assert(uc3b_uart_available() == 0);
    }
    receive(0xbb, UC3B_UART_ERRORS);
    fake_status |= UC3B_UART_ERRORS; /* Sticky error even without a ready byte. */
    gm = 1; uc3b_uart_isr(); gm = 0;
    assert(clear_count == 5 && !(fake_status & UC3B_UART_ERRORS));
    /* Random interleavings vs an independent queue, including many wrap/full
     * transitions, zero bytes, and 0xff. One producer and one consumer. */
    for (round = 0; round < 30000; ++round) {
        rng = rng * 1664525U + 1013904223U;
        if ((rng >> 16) & 1) {
            uint8_t value = (uint8_t)(rng >> 24);
            receive(value, 0);
            if (count < SERIAL_RX_BUFFER_SIZE - 1) { reference[count++] = value; }
        } else {
            assert(uc3b_uart_peek() == (count ? reference[0] : -1));
            assert(uc3b_uart_read() == (count ? reference[0] : -1));
            if (count) {
                --count;
                for (i = 0; i < count; ++i) { reference[i] = reference[i + 1]; }
            }
        }
        assert(uc3b_uart_available() == (int)count && !gm);
    }
    fake_status = 0; status_reads = 0; ready_after = 3; empty_after = 9;
    assert(uc3b_uart_write(0x80) && sent == 0x80 && status_reads == 3);
    uc3b_uart_flush();
    assert(status_reads == 9 && uc3b_uart_available() == (int)count && !gm);
    gm = 1;
    uc3b_uart_end();
    assert(gm && !enabled && !uc3b_uart_active() && uc3b_uart_available() == 0);
    assert(uc3b_uart_init(&config) && gm); /* Never globally unmasks caller. */
    gm = 0;
    receive(255, 0); assert(uc3b_uart_read() == 255);
    receive(0, 0); assert(uc3b_uart_read() == 0);
    config.route = UC3B_UART_UNCONFIGURED;
    assert(!uc3b_uart_init(&config) && !enabled && !uc3b_uart_active());
    config.route = UC3B_UART_U0C; config.baudrate = 0;
    assert(!uc3b_uart_init(&config));
    puts("PASS: UART baud bounds/error, RX wrap/overflow/errors, TXEMPTY flush and lifecycle");
    return 0;
}
