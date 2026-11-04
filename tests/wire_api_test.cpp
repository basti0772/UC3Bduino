/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <Wire.h>
#include "hal/uc3b_i2c.h"
static bool active, last_stop, timeout_flag;
static uint8_t next_error, last_address, copied[32];
static size_t last_count;
static unsigned int transfers;
bool uc3b_i2c_begin(void) { active = true; return true; }
void uc3b_i2c_end(void) { active = false; }
bool uc3b_i2c_set_clock(uint32_t hz) { return active && hz <= 100000 && hz >= 1000; }
bool uc3b_i2c_set_timeout(uint32_t us) { return us > 0 && us <= 1000000; }
bool uc3b_i2c_active(void) { return active; }
bool uc3b_i2c_timeout_flag(void) { return timeout_flag; }
void uc3b_i2c_clear_timeout(void) { timeout_flag = false; }
uint8_t uc3b_i2c_write(uint8_t address, const uint8_t *data, size_t count, bool stop)
{
    ++transfers; last_address = address; last_count = count; last_stop = stop;
    for (size_t i = 0; i < count; ++i) { copied[i] = data[i]; }
    return next_error;
}
uint8_t uc3b_i2c_read(uint8_t address, uint8_t *data, size_t count, bool stop)
{
    ++transfers; last_address = address; last_count = count; last_stop = stop;
    for (size_t i = 0; i < count; ++i) { data[i] = (uint8_t)(i + 0x80); }
    return next_error;
}
#include "../arduino/hardware/uc3bduino/avr32/cores/uc3bduino/Wire.cpp"
int main(void)
{
    assert(!Wire && Wire.read() == -1 && Wire.peek() == -1);
    assert(Wire.write((uint8_t)1) == 0 && Wire.lastError() == 4);
    Wire.begin(); assert(Wire); Wire.setClock(400000); assert(Wire.lastError() == 4);
    Wire.setClock(100000); assert(Wire);
    Wire.beginTransmission(0x50); assert(Wire.endTransmission() == 0 && last_count == 0);
    Wire.beginTransmission(0x50); assert(Wire.write(0) == 1 && Wire.write(255UL) == 1);
    assert(Wire.endTransmission() == 0 && copied[0] == 0 && copied[1] == 255);
    uint8_t data[] = { 0, 0x55, 0xff };
    Wire.beginTransmission(0x50); assert(Wire.write(data, 3) == 3);
    assert(Wire.endTransmission(false) == 0 && !last_stop && last_count == 3);
    assert(copied[0] == 0 && copied[1] == 0x55 && copied[2] == 0xff);
    assert(Wire.requestFrom(0x50, 3) == 3 && last_stop && last_address == 0x50);
    assert(Wire.available() == 3 && Wire.peek() == 0x80 && Wire.peek() == 0x80);
    assert(Wire.read() == 0x80 && Wire.read() == 0x81 && Wire.read() == 0x82);
    assert(Wire.available() == 0 && Wire.read() == -1);
    unsigned int before = transfers;
    assert(Wire.requestFrom(0x50, 33) == 0 && transfers == before);
    Wire.beginTransmission(0x78); assert(Wire.endTransmission() == 4 && transfers == before);
    Wire.beginTransmission(0x50);
    for (int i = 0; i < 32; ++i) { assert(Wire.write((uint8_t)i) == 1); }
    assert(Wire.write((uint8_t)32) == 0 && Wire.endTransmission() == 1 && transfers == before);
    Wire.beginTransmission(0x50); assert(Wire.write((const uint8_t *)0, 1) == 0);
    assert(Wire.endTransmission() == 4 && transfers == before);
    Wire.beginTransmission(0x50); assert(Wire.requestFrom(0x50, 1) == 0 && transfers == before);
    Wire.beginTransmission(0x50); assert(Wire.write("abc") == 3);
    assert(Wire.endTransmission() == 0 && copied[0] == 'a' && copied[2] == 'c');
    for (uint8_t code = 2; code <= 5; ++code) {
        next_error = code;
        Wire.beginTransmission(0x50); assert(Wire.endTransmission() == code);
        assert(Wire.requestFrom(0x50, 3) == 0 && Wire.available() == 0 && Wire.read() == -1);
    }
    Wire.setWireTimeout(0); assert(Wire.lastError() == 4);
    Wire.setWireTimeout(); assert(Wire.lastError() == 0);
    timeout_flag = true; assert(Wire.getWireTimeoutFlag()); Wire.clearWireTimeoutFlag();
    assert(!Wire.getWireTimeoutFlag()); Wire.flush(); Wire.end(); assert(!Wire);
    puts("PASS: Wire buffers, real probe forwarding, repeated-start flags, overflow, partial-read suppression, errors and timeout API");
    return 0;
}
