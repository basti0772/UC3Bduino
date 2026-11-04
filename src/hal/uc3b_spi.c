/* SPDX-License-Identifier: MIT */
#include "uc3b_spi.h"
#include "uc3b_clock.h"
#include "uc3b_spi_hw.h"

#define SPI_POLL_LIMIT 100000UL
static bool started, configured, lsb;
static uint8_t error_code;
static uint32_t actual_hz, timeout_cycles;

static bool spi_fail(uint8_t error)
{
    if (error_code == UC3B_SPI_OK) { error_code = error; }
    return false;
}
static bool spi_wait(uint32_t mask)
{
    uint32_t count = spi_hw_count(), remaining = SPI_POLL_LIMIT;
    do {
        uint32_t status = spi_hw_status();
        if ((status & SPI_HW_ERRORS) || !(status & SPI_HW_ENABLED)) {
            return spi_fail(UC3B_SPI_HARDWARE);
        }
        if ((status & mask) == mask) { return true; }
        if ((uint32_t)(spi_hw_count() - count) >= timeout_cycles) { break; }
    } while (--remaining);
    return spi_fail(UC3B_SPI_TIMEOUT);
}
static void spi_abort(void)
{
    spi_hw_reset();
    spi_hw_pins(false);
    started = configured = false;
    actual_hz = 0;
}
static uint8_t spi_reverse(uint8_t b)
{
    b = (uint8_t)(((b & 0x55U) << 1) | ((b >> 1) & 0x55U));
    b = (uint8_t)(((b & 0x33U) << 2) | ((b >> 2) & 0x33U));
    return (uint8_t)((b << 4) | (b >> 4));
}
bool uc3b_spi_begin(void)
{
    uint32_t count, remaining = SPI_POLL_LIMIT, cpu = uc3b_clock_get_cpu_hz();
    if (!spi_hw_foreground()) { return spi_fail(UC3B_SPI_CONTEXT); }
    if (started && configured) { return true; } /* Idempotent; no ownership refcount. */
    if (started) { spi_abort(); } /* Recover a rejected configuration with defaults. */
    error_code = UC3B_SPI_OK;
    if (cpu < 100000UL || cpu > 60000000UL) { return spi_fail(UC3B_SPI_ARGUMENT); }
    spi_hw_clock_enable();
    count = spi_hw_count();
    while (!spi_hw_clock_ready()) {
        if (--remaining == 0 || (uint32_t)(spi_hw_count() - count) >= cpu / 100UL) {
            return spi_fail(UC3B_SPI_TIMEOUT); /* No SPI MMIO without clock. */
        }
    }
    spi_hw_reset();
    spi_hw_mode(false);
    /* Configure a valid divider BEFORE enabling/connecting the controller. */
    started = true;
    if (!uc3b_spi_configure(4000000UL, false, 0)) { spi_abort(); return false; }
    spi_hw_pins(true);
    spi_hw_enable();
    if (!spi_wait(SPI_HW_EMPTY)) { spi_abort(); return false; }
    return true;
}
void uc3b_spi_end(void)
{
    if (!spi_hw_foreground()) { (void)spi_fail(UC3B_SPI_CONTEXT); return; }
    if (started) { spi_abort(); } /* Every transfer already waited for TXEMPTY. */
}
bool uc3b_spi_configure(uint32_t hz, bool lsb_first, uint8_t mode)
{
    uint32_t pba, cpu, divider, csr;
    if (!spi_hw_foreground()) { return spi_fail(UC3B_SPI_CONTEXT); }
    if (!started) { return spi_fail(UC3B_SPI_NOT_STARTED); }
    pba = uc3b_clock_get_pba_hz(); cpu = uc3b_clock_get_cpu_hz();
    configured = false; actual_hz = 0;
    if (!hz || mode > 3 || pba < 100000UL || pba > cpu || cpu > 60000000UL) {
        return spi_fail(UC3B_SPI_ARGUMENT);
    }
    divider = pba / hz + (pba % hz != 0); /* Ceiling without overflow. */
    if (divider < 2) { divider = 2; } /* Conservative PBA/2; never SCBR=1. */
    if (divider > 255) { return spi_fail(UC3B_SPI_ARGUMENT); } /* Never overspeed. */
    csr = (divider << 8) | ((mode & 2) ? 1UL : 0) | ((mode & 1) ? 0 : 2UL);
    spi_hw_configure(csr); /* 8 bits, no hardware CS hold or delays. */
    timeout_cycles = ((cpu + pba - 1) / pba) * divider * 8UL + cpu / 100UL;
    actual_hz = pba / divider;
    lsb = lsb_first; configured = true;
    error_code = UC3B_SPI_OK;
    return true;
}
bool uc3b_spi_transfer(uint8_t tx, uint8_t *rx)
{
    uint8_t value;
    if (!spi_hw_foreground()) { return spi_fail(UC3B_SPI_CONTEXT); }
    if (!rx) { return spi_fail(UC3B_SPI_ARGUMENT); }
    if (!started || !configured) { return spi_fail(UC3B_SPI_NOT_STARTED); }
    if (error_code) { return false; }
    if (!spi_wait(SPI_HW_TDRE | SPI_HW_EMPTY)) { spi_abort(); return false; }
    (void)spi_hw_read(); /* Discard any stale receive holding register. */
    spi_hw_write(lsb ? spi_reverse(tx) : tx);
    if (!spi_wait(SPI_HW_RDRF)) { spi_abort(); return false; }
    value = spi_hw_read();
    if (!spi_wait(SPI_HW_EMPTY)) { spi_abort(); return false; }
    *rx = lsb ? spi_reverse(value) : value;
    return true;
}
bool uc3b_spi_loopback(bool enabled)
{
    if (!spi_hw_foreground()) { return spi_fail(UC3B_SPI_CONTEXT); }
    if (!started || !configured) { return spi_fail(UC3B_SPI_NOT_STARTED); }
    if (!spi_wait(SPI_HW_EMPTY)) { spi_abort(); return false; }
    spi_hw_mode(enabled);
    return true;
}
bool uc3b_spi_active(void) { return started && configured; }
uint32_t uc3b_spi_clock_hz(void) { return actual_hz; }
uint8_t uc3b_spi_error(void) { return error_code; }
void uc3b_spi_clear_error(void) { error_code = UC3B_SPI_OK; }
