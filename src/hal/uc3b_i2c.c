/* SPDX-License-Identifier: MIT */
#include "uc3b_i2c.h"
#include "uc3b_clock.h"
#include "uc3b_i2c_hw.h"
#define I2C_POLL_LIMIT 100000UL
static bool active, held, timed_out, clock_valid;
static uint32_t half_cycles, cpu_hz, timeout_us, deadline_start, deadline_cycles;
static uint8_t result;

static void release_bus(void)
{
    i2c_hw_sda(true); i2c_hw_scl(true); held = false;
}
static bool fail(uint8_t code)
{
    result = code;
    if (code == UC3B_I2C_TIMEOUT) { timed_out = true; }
    return false;
}
static bool expired(void)
{
    if ((uint32_t)(i2c_hw_count() - deadline_start) >= deadline_cycles) {
        (void)fail(UC3B_I2C_TIMEOUT); return true;
    }
    return false;
}
static bool pause_half(void)
{
    uint32_t start = i2c_hw_count(), polls = I2C_POLL_LIMIT;
    while ((uint32_t)(i2c_hw_count() - start) < half_cycles) {
        if (expired()) { return false; }
        if (--polls == 0) { return fail(UC3B_I2C_TIMEOUT); }
    }
    return !expired();
}
static bool raise_clock(void)
{
    uint32_t polls = I2C_POLL_LIMIT;
    i2c_hw_scl(true);
    while (!i2c_hw_read_scl()) {
        if (expired()) { return false; }
        if (--polls == 0) { return fail(UC3B_I2C_TIMEOUT); }
    }
    return pause_half(); /* High interval starts after stretch has finished. */
}
static bool start_bus(void)
{
    i2c_hw_sda(true);
    if (!pause_half() || !raise_clock()) { return false; }
    if (!i2c_hw_read_sda()) { return fail(UC3B_I2C_OTHER); } /* Busy bus. */
    i2c_hw_sda(false);
    if (!pause_half()) { return false; }
    i2c_hw_scl(false); held = true;
    return true;
}
static bool stop_bus(void)
{
    i2c_hw_scl(false); i2c_hw_sda(false);
    if (!pause_half() || !raise_clock()) { return false; }
    i2c_hw_sda(true);
    if (!pause_half()) { return false; }
    held = false;
    return i2c_hw_read_sda() || fail(UC3B_I2C_OTHER);
}
static bool write_bit(bool one)
{
    i2c_hw_sda(one);
    if (!pause_half() || !raise_clock()) { return false; }
    /* Never drive HIGH against a competing master or stuck slave. */
    if (one && !i2c_hw_read_sda()) { return fail(UC3B_I2C_OTHER); }
    i2c_hw_scl(false);
    return true;
}
static bool read_bit(bool *one)
{
    i2c_hw_sda(true);
    if (!pause_half() || !raise_clock()) { return false; }
    *one = i2c_hw_read_sda();
    i2c_hw_scl(false);
    return true;
}
static bool write_byte(uint8_t value, uint8_t nack)
{
    unsigned int bit; bool ack_high;
    for (bit = 0; bit < 8; ++bit) {
        if (!write_bit((value & 0x80) != 0)) { return false; }
        value <<= 1;
    }
    if (!read_bit(&ack_high)) { return false; }
    if (ack_high) { return fail(nack); }
    return true;
}
static bool read_byte(uint8_t *value, bool last)
{
    unsigned int bit; uint8_t data = 0; bool one;
    for (bit = 0; bit < 8; ++bit) {
        if (!read_bit(&one)) { return false; }
        data = (uint8_t)((data << 1) | (one ? 1 : 0));
    }
    if (!write_bit(last)) { return false; } /* ACK all but last, then NACK. */
    i2c_hw_sda(true);
    *value = data;
    return true;
}
static bool prepare(uint8_t address)
{
    if (!i2c_hw_foreground()) { return fail(UC3B_I2C_OTHER); }
    /* Standard 7-bit unicast addresses only. No general-call/reserved probes. */
    if (!active || !clock_valid || address < 8 || address > 0x77) {
        return fail(UC3B_I2C_OTHER);
    }
    result = UC3B_I2C_OK;
    deadline_start = i2c_hw_count();
    /* Ceiling CPU cycles/us, <=60 MHz and <=1 second; cannot overflow. */
    deadline_cycles = ((cpu_hz + 999999UL) / 1000000UL) * timeout_us;
    return true;
}
static uint8_t finish(bool success, bool stop)
{
    if (success) {
        if (!stop || stop_bus()) { return UC3B_I2C_OK; }
    } else if (held && (result == UC3B_I2C_ADDRESS_NACK || result == UC3B_I2C_DATA_NACK)) {
        /* NACK belongs to us: attempt STOP within the same total deadline. */
        (void)stop_bus();
    }
    release_bus(); /* Timeout/arbitration: release only, never force HIGH/clock recovery. */
    return result;
}
bool uc3b_i2c_begin(void)
{
    if (!i2c_hw_foreground()) { return false; }
    if (active) { return clock_valid; }
    cpu_hz = uc3b_clock_get_cpu_hz();
    /* Software timing on nominal RC is too coarse; do not pretend 100 kHz. */
    if (cpu_hz < 1000000UL || cpu_hz > 60000000UL) { return false; }
    i2c_hw_init();
    active = true; held = false; timed_out = false; timeout_us = 25000UL;
    return uc3b_i2c_set_clock(100000UL);
}
void uc3b_i2c_end(void)
{
    if (!i2c_hw_foreground() || !active) { return; }
    if (held && prepare(8)) { (void)stop_bus(); }
    release_bus(); active = clock_valid = false;
}
bool uc3b_i2c_set_clock(uint32_t hz)
{
    if (!i2c_hw_foreground() || !active || held) { return false; }
    clock_valid = false;
    if (hz < 1000UL || hz > 100000UL) { return false; }
    half_cycles = cpu_hz / (2UL * hz) + (cpu_hz % (2UL * hz) != 0);
    clock_valid = true;
    return true;
}
bool uc3b_i2c_set_timeout(uint32_t us)
{
    if (!i2c_hw_foreground() || !active || held || !us || us > 1000000UL) { return false; }
    timeout_us = us; return true;
}
uint8_t uc3b_i2c_write(uint8_t address, const uint8_t *data, size_t count, bool stop)
{
    size_t i; bool ok;
    if (count > 32 || (count && !data)) { return UC3B_I2C_OTHER; }
    if (!prepare(address)) { return result; }
    ok = start_bus() && write_byte((uint8_t)(address << 1), UC3B_I2C_ADDRESS_NACK);
    for (i = 0; i < count && ok; ++i) { ok = write_byte(data[i], UC3B_I2C_DATA_NACK); }
    return finish(ok, stop);
}
uint8_t uc3b_i2c_read(uint8_t address, uint8_t *data, size_t count, bool stop)
{
    size_t i; bool ok;
    if (count > 32 || (count && !data)) { return UC3B_I2C_OTHER; }
    if (!count) { return UC3B_I2C_OK; }
    if (!prepare(address)) { return result; }
    ok = start_bus() && write_byte((uint8_t)((address << 1) | 1), UC3B_I2C_ADDRESS_NACK);
    for (i = 0; i < count && ok; ++i) { ok = read_byte(&data[i], i + 1 == count); }
    return finish(ok, stop);
}
bool uc3b_i2c_timeout_flag(void) { return timed_out; }
void uc3b_i2c_clear_timeout(void) { timed_out = false; }
bool uc3b_i2c_active(void) { return active && clock_valid; }
