/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#define UC3B_I2C_HW_H
static uint32_t mock_cpu = 12000000, ticks;
static bool foreground = true, scl_release = true, sda_release = true;
static bool frozen, stuck_scl;
static unsigned int init_calls, starts, stops, edges, sample_count, sample_index, stretch;
static bool samples[4096], sent_bits[4096];
uint32_t uc3b_clock_get_cpu_hz(void) { return mock_cpu; }
static bool i2c_hw_foreground(void) { return foreground; }
static uint32_t i2c_hw_count(void) { if (!frozen) { ++ticks; } return ticks; }
static void i2c_hw_scl(bool release)
{
    if (release && !scl_release) { assert(edges < 4096); sent_bits[edges++] = sda_release; }
    scl_release = release;
}
static void i2c_hw_sda(bool release)
{
    if (scl_release && release != sda_release) { if (release) { ++stops; } else { ++starts; } }
    sda_release = release;
}
static bool i2c_hw_read_scl(void)
{
    if (stretch) { --stretch; return false; }
    return scl_release && !stuck_scl;
}
static bool i2c_hw_read_sda(void)
{
    assert(sample_index < sample_count);
    return samples[sample_index++];
}
static void i2c_hw_init(void) { ++init_calls; scl_release = sda_release = true; }
#include "../src/hal/uc3b_i2c.c"
static void reset_trace(void)
{
    starts = stops = edges = sample_count = sample_index = 0;
}
static void sample(bool high) { assert(sample_count < 4096); samples[sample_count++] = high; }
static void slave_ack(uint8_t value, bool nack)
{
    unsigned int bit;
    for (bit = 0; bit < 8; ++bit) {
        if (value & (0x80U >> bit)) { sample(true); } /* Transmitted ones must stay HIGH. */
    }
    sample(nack);
}
static void slave_data(uint8_t value)
{
    unsigned int bit;
    for (bit = 0; bit < 8; ++bit) { sample((value & (0x80U >> bit)) != 0); }
}
static void verify_byte(unsigned int edge, uint8_t value)
{
    unsigned int bit;
    for (bit = 0; bit < 8; ++bit) { assert(sent_bits[edge + bit] == ((value & (0x80U >> bit)) != 0)); }
    assert(sent_bits[edge + 8]); /* Release SDA for peer ACK. */
}
int main(void)
{
    uint8_t tx[] = { 0x00, 0xa5, 0xff }, rx[32]; unsigned int before, i;
    foreground = false; assert(!uc3b_i2c_begin() && init_calls == 0); foreground = true;
    mock_cpu = 115200; assert(!uc3b_i2c_begin() && init_calls == 0); mock_cpu = 12000000;
    assert(uc3b_i2c_begin() && scl_release && sda_release);
    before = init_calls; assert(uc3b_i2c_begin() && init_calls == before);
    assert(!uc3b_i2c_set_clock(400000) && !uc3b_i2c_active());
    assert(uc3b_i2c_set_clock(100000) && uc3b_i2c_active());
    assert(!uc3b_i2c_set_clock(0)); assert(uc3b_i2c_set_clock(1000));
    assert(uc3b_i2c_set_clock(100000));
    assert(!uc3b_i2c_set_timeout(0) && !uc3b_i2c_set_timeout(1000001));
    assert(uc3b_i2c_set_timeout(25000));
    assert(uc3b_i2c_write(0, tx, 1, true) == 4 && starts == 0);
    assert(uc3b_i2c_write(0x78, tx, 1, true) == 4 && starts == 0);
    assert(uc3b_i2c_write(0x50, 0, 1, true) == 4 && starts == 0);
    assert(uc3b_i2c_read(0x50, rx, 33, true) == 4 && starts == 0);
    sample(true); slave_ack(0xa0, false); sample(true);
    assert(uc3b_i2c_write(0x50, 0, 0, true) == 0); /* Real address-only probe. */
    assert(starts == 1 && stops == 1 && edges == 10 && sample_index == sample_count);
    verify_byte(0, 0xa0);
    reset_trace(); sample(true); slave_ack(0xa0, false);
    for (i = 0; i < 3; ++i) { slave_ack(tx[i], false); }
    sample(true);
    assert(uc3b_i2c_write(0x50, tx, 3, true) == 0);
    assert(starts == 1 && stops == 1 && edges == 37 && sample_index == sample_count);
    for (i = 0; i < 3; ++i) { verify_byte(9 * (i + 1), tx[i]); }
    reset_trace(); sample(true); slave_ack(0xa0, false); slave_ack(0, false);
    assert(uc3b_i2c_write(0x50, tx, 1, false) == 0);
    assert(starts == 1 && stops == 0 && !scl_release && sda_release);
    assert(!uc3b_i2c_set_clock(10000) && !uc3b_i2c_set_timeout(1000));
    sample(true); slave_ack(0xa1, false); slave_data(0x96); slave_data(0x81);
    sample(true); sample(true); /* Master NACK, then STOP checks released SDA. */
    assert(uc3b_i2c_read(0x50, rx, 2, true) == 0 && rx[0] == 0x96 && rx[1] == 0x81);
    assert(starts == 2 && stops == 1 && sample_index == sample_count);
    assert(!sent_bits[36] && sent_bits[45]); /* ACK first read, NACK final read. */
    reset_trace(); sample(true); slave_ack(0xa0, true); sample(true);
    assert(uc3b_i2c_write(0x50, tx, 3, true) == 2 && stops == 1);
    reset_trace(); sample(true); slave_ack(0xa0, false); slave_ack(0, true); sample(true);
    assert(uc3b_i2c_write(0x50, tx, 3, true) == 3 && stops == 1);
    reset_trace(); sample(false);
    assert(uc3b_i2c_write(0x50, tx, 1, true) == 4 && starts == 0); /* Busy SDA. */
    reset_trace(); sample(true); sample(false); /* Contention on transmitted address MSB. */
    assert(uc3b_i2c_write(0x50, tx, 1, true) == 4 && stops == 0 && scl_release && sda_release);
    reset_trace(); sample(true); slave_ack(0xa0, false); sample(true);
    stretch = 100; ticks = UINT32_MAX - 30;
    assert(uc3b_i2c_write(0x50, 0, 0, true) == 0 && sample_index == sample_count);
    reset_trace(); stuck_scl = true;
    assert(uc3b_i2c_write(0x50, tx, 1, true) == 5 && uc3b_i2c_timeout_flag());
    assert(scl_release && sda_release); stuck_scl = false; uc3b_i2c_clear_timeout();
    assert(!uc3b_i2c_timeout_flag()); frozen = true;
    assert(uc3b_i2c_write(0x50, tx, 1, true) == 5 && scl_release && sda_release);
    frozen = false; uc3b_i2c_clear_timeout();
    reset_trace(); sample(true); slave_ack(0xa0, false); sample(true);
    assert(uc3b_i2c_write(0x50, 0, 0, true) == 0); /* Retry after timeout. */
    reset_trace(); sample(true); slave_ack(0xa0, false);
    assert(uc3b_i2c_write(0x50, 0, 0, false) == 0);
    sample(true); uc3b_i2c_end(); assert(stops == 1 && !uc3b_i2c_active());
    assert(uc3b_i2c_begin()); uc3b_i2c_end();
    puts("PASS: I2C wire-level address/data/ACK/NACK, repeated START, STOP, stretch, contention, COUNT wrap/stall, timeout and recovery");
    return 0;
}
