/* SPDX-License-Identifier: MIT */
#include "Wire.h"
#include "hal/uc3b_i2c.h"
TwoWire Wire;

void TwoWire::begin()
{
    uc3b_i2c_end(); /* Finish a previously held transaction before resetting buffers. */
    tx_size_ = rx_size_ = rx_index_ = 0;
    transmitting_ = overflow_ = false;
    error_ = (uint8_t)(uc3b_i2c_begin() ? UC3B_I2C_OK : UC3B_I2C_OTHER);
}
void TwoWire::end()
{
    uc3b_i2c_end();
    tx_size_ = rx_size_ = rx_index_ = 0;
    transmitting_ = overflow_ = false;
}
void TwoWire::setClock(uint32_t hz)
{
    if (transmitting_) { error_ = UC3B_I2C_OTHER; return; }
    error_ = (uint8_t)(uc3b_i2c_set_clock(hz) ? UC3B_I2C_OK : UC3B_I2C_OTHER);
}
void TwoWire::beginTransmission(uint8_t address)
{
    address_ = address; tx_size_ = 0; overflow_ = false; transmitting_ = true;
    error_ = (uint8_t)(uc3b_i2c_active() && address >= 8 && address <= 0x77 ? UC3B_I2C_OK : UC3B_I2C_OTHER);
}
size_t TwoWire::write(uint8_t value)
{
    if (!transmitting_ || error_) { if (!error_) { error_ = UC3B_I2C_OTHER; } return 0; }
    if (tx_size_ == WIRE_BUFFER_LENGTH) { overflow_ = true; error_ = UC3B_I2C_OVERFLOW; return 0; }
    tx_[tx_size_++] = value; return 1;
}
size_t TwoWire::write(const uint8_t *data, size_t count)
{
    size_t written = 0;
    if (!data && count) { error_ = UC3B_I2C_OTHER; return 0; }
    while (written < count && write(data[written])) { ++written; }
    return written;
}
size_t TwoWire::write(const char *text)
{
    size_t written = 0;
    if (!text) { error_ = UC3B_I2C_OTHER; return 0; }
    while (*text && write((uint8_t)*text++)) { ++written; }
    return written;
}
uint8_t TwoWire::endTransmission(bool sendStop)
{
    if (!transmitting_) { error_ = UC3B_I2C_OTHER; return error_; }
    transmitting_ = false;
    if (overflow_) { error_ = UC3B_I2C_OVERFLOW; }
    if (!error_) { error_ = uc3b_i2c_write(address_, tx_, tx_size_, sendStop); }
    tx_size_ = 0;
    return error_;
}
uint8_t TwoWire::requestFrom(uint8_t address, size_t count, bool sendStop)
{
    rx_size_ = rx_index_ = 0;
    if (transmitting_ || count > WIRE_BUFFER_LENGTH) { error_ = UC3B_I2C_OTHER; return 0; }
    error_ = uc3b_i2c_read(address, rx_, count, sendStop);
    if (!error_) { rx_size_ = (uint8_t)count; }
    return rx_size_; /* Hide partially received data if the operation failed. */
}
int TwoWire::available() { return rx_size_ - rx_index_; }
int TwoWire::read() { return available() ? rx_[rx_index_++] : -1; }
int TwoWire::peek() { return available() ? rx_[rx_index_] : -1; }
void TwoWire::flush() { }
void TwoWire::setWireTimeout(uint32_t timeout)
{
    if (transmitting_) { error_ = UC3B_I2C_OTHER; return; }
    error_ = (uint8_t)(uc3b_i2c_set_timeout(timeout) ? UC3B_I2C_OK : UC3B_I2C_OTHER);
}
bool TwoWire::getWireTimeoutFlag() { return uc3b_i2c_timeout_flag(); }
void TwoWire::clearWireTimeoutFlag() { uc3b_i2c_clear_timeout(); }
uint8_t TwoWire::lastError() const { return error_; }
TwoWire::operator bool() const { return uc3b_i2c_active() && error_ == UC3B_I2C_OK; }
