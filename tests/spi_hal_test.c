/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#define UC3B_SPI_HW_H
#define SPI_HW_TDRE 2UL
#define SPI_HW_RDRF 1UL
#define SPI_HW_EMPTY 0x200UL
#define SPI_HW_ENABLED 0x10000UL
#define SPI_HW_ERRORS 0x0cUL
static uint32_t mock_cpu = 12000000, mock_pba = 12000000, tick, mock_status, mock_csr, status_reads;
static unsigned int resets, writes, clock_calls, pins_on, pins_off, configurations;
static bool foreground = true, clock_ready = true, frozen_count, no_receive, no_empty, fault;
static bool loopback;
static uint8_t raw_tx, raw_rx;
uint32_t uc3b_clock_get_cpu_hz(void) { return mock_cpu; }
uint32_t uc3b_clock_get_pba_hz(void) { return mock_pba; }
static bool spi_hw_foreground(void) { return foreground; }
static uint32_t spi_hw_count(void) { if (!frozen_count) { tick += 100; } return tick; }
static void spi_hw_clock_enable(void) { ++clock_calls; }
static bool spi_hw_clock_ready(void) { return clock_ready; }
static void spi_hw_reset(void) { assert(clock_ready); ++resets; mock_status = 0; }
static void spi_hw_mode(bool value) { loopback = value; }
static void spi_hw_configure(uint32_t value) { ++configurations; mock_csr = value; }
static void spi_hw_enable(void) { mock_status = SPI_HW_ENABLED | SPI_HW_TDRE | SPI_HW_EMPTY; }
static uint32_t spi_hw_status(void) { ++status_reads; return mock_status; }
static void spi_hw_write(uint8_t value)
{
    ++writes; raw_tx = value; raw_rx = (uint8_t)(value ^ 0x12);
    mock_status = SPI_HW_ENABLED | SPI_HW_TDRE | (no_receive ? 0 : SPI_HW_RDRF)
        | (no_empty ? 0 : SPI_HW_EMPTY) | (fault ? SPI_HW_ERRORS : 0);
}
static uint8_t spi_hw_read(void) { mock_status &= ~SPI_HW_RDRF; return raw_rx; }
static void spi_hw_pins(bool enable) { if (enable) { ++pins_on; } else { ++pins_off; } }
#include "../src/hal/uc3b_spi.c"

static uint8_t reversed(uint8_t value)
{
    unsigned int i; uint8_t result = 0;
    for (i = 0; i < 8; ++i) { result = (uint8_t)((result << 1) | (value & 1)); value >>= 1; }
    return result;
}
int main(void)
{
    uint8_t rx = 0x33;
    unsigned int i, m, before;
    assert(!uc3b_spi_transfer(0, &rx) && writes == 0 && rx == 0x33);
    foreground = false;
    assert(!uc3b_spi_begin() && clock_calls == 0);
    foreground = true; clock_ready = false;
    assert(!uc3b_spi_begin() && resets == 0 && pins_on == 0);
    clock_ready = true;
    assert(uc3b_spi_begin() && mock_csr == 0x302 && pins_on == 1);
    before = resets;
    assert(uc3b_spi_begin() && resets == before);
    for (m = 0; m < 4; ++m) {
        assert(uc3b_spi_configure(1000000, false, (uint8_t)m));
        assert(mock_csr == (12UL << 8) + (m >= 2 ? 1UL : 0) + (m % 2 ? 0 : 2UL));
    }
    assert(uc3b_spi_configure(UINT32_MAX, false, 0) && mock_csr == 0x202);
    assert(uc3b_spi_clock_hz() == 6000000);
    assert(uc3b_spi_configure(5000000, false, 0) && uc3b_spi_clock_hz() == 4000000);
    assert(uc3b_spi_configure(47059, false, 0) && (mock_csr >> 8) == 255);
    before = configurations;
    assert(!uc3b_spi_configure(47058, false, 0) && configurations == before);
    before = writes;
    uc3b_spi_clear_error();
    assert(!uc3b_spi_transfer(0, &rx) && writes == before); /* Not stale settings. */
    assert(uc3b_spi_begin() && uc3b_spi_clock_hz() == 4000000 && !uc3b_spi_error());
    assert(!uc3b_spi_configure(0, false, 0));
    assert(!uc3b_spi_configure(1000000, false, 4));
    mock_pba = 0; assert(!uc3b_spi_configure(1000000, false, 0)); mock_pba = 12000000;
    mock_cpu = 6000000; assert(!uc3b_spi_configure(1000000, false, 0)); mock_cpu = 12000000;
    assert(uc3b_spi_configure(1000000, false, 0));
    for (i = 0; i < 256; ++i) {
        assert(uc3b_spi_transfer((uint8_t)i, &rx));
        assert(raw_tx == i && rx == (i ^ 0x12));
    }
    assert(uc3b_spi_configure(1000000, true, 0));
    for (i = 0; i < 256; ++i) {
        assert(uc3b_spi_transfer((uint8_t)i, &rx));
        assert(raw_tx == reversed((uint8_t)i) && rx == (i ^ 0x48));
    }
    assert(uc3b_spi_loopback(true) && loopback);
    assert(uc3b_spi_loopback(false) && !loopback);
    before = writes; foreground = false;
    assert(!uc3b_spi_transfer(0, &rx) && writes == before);
    uc3b_spi_end(); assert(uc3b_spi_active()); foreground = true;
    assert(uc3b_spi_configure(1000000, false, 0));
    assert(!uc3b_spi_transfer(0, 0) && writes == before);
    uc3b_spi_clear_error();
    tick = UINT32_MAX - 500; no_receive = true;
    assert(!uc3b_spi_transfer(0, &rx) && uc3b_spi_error() == UC3B_SPI_TIMEOUT);
    assert(!uc3b_spi_active() && pins_off != 0 && uc3b_spi_clock_hz() == 0);
    no_receive = false;
    assert(uc3b_spi_begin());
    no_empty = true;
    assert(!uc3b_spi_transfer(0, &rx) && uc3b_spi_error() == UC3B_SPI_TIMEOUT);
    no_empty = false;
    assert(uc3b_spi_begin());
    fault = true;
    assert(!uc3b_spi_transfer(0, &rx) && uc3b_spi_error() == UC3B_SPI_HARDWARE);
    fault = false;
    assert(uc3b_spi_begin());
    mock_status &= ~SPI_HW_ENABLED; /* TDRE may still be set on affected silicon. */
    before = writes;
    assert(!uc3b_spi_transfer(0, &rx) && writes == before);
    assert(uc3b_spi_begin());
    no_receive = frozen_count = true; status_reads = 0;
    assert(!uc3b_spi_transfer(0, &rx) && status_reads <= SPI_POLL_LIMIT + 2);
    no_receive = frozen_count = false;
    mock_cpu = mock_pba = 115200;
    assert(uc3b_spi_begin() && uc3b_spi_clock_hz() == 57600);
    assert(uc3b_spi_configure(452, false, 0) && (mock_csr >> 8) == 255);
    assert(uc3b_spi_transfer(0x55, &rx));
    uc3b_spi_end(); before = resets; uc3b_spi_end();
    assert(!uc3b_spi_active() && resets == before);
    puts("PASS: SPI modes/dividers, all-byte bit reversal, timeout/wrap/frozen counter, errors, context and recovery (mock MMIO)");
    return 0;
}
