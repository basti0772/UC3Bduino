/* SPDX-License-Identifier: MIT */
#include "SPI.h"
SPIClass SPI; /* Trivial object, initialized by the existing BSS startup. */

void SPIClass::begin()
{
    if (transaction_) { api_error_ = UC3B_SPI_ARGUMENT; return; }
    if (uc3b_spi_active() && settings_valid_) { return; }
    if (uc3b_spi_active()) { uc3b_spi_end(); }
    api_error_ = UC3B_SPI_OK; lsb_ = false;
    settings_valid_ = uc3b_spi_begin();
}
void SPIClass::end()
{
    uc3b_spi_end();
    transaction_ = settings_valid_ = false;
}
void SPIClass::beginTransaction(SPISettings settings)
{
    if (transaction_) { api_error_ = UC3B_SPI_ARGUMENT; settings_valid_ = false; return; }
    transaction_ = true;
    settings_valid_ = false;
    api_error_ = UC3B_SPI_OK;
    if (settings.order_ > MSBFIRST || (settings.mode_ & ~0x0c) != 0) {
        api_error_ = UC3B_SPI_ARGUMENT; return;
    }
    if (uc3b_spi_configure(settings.clock_, settings.order_ == LSBFIRST,
                           settings.mode_ >> 2)) {
        lsb_ = settings.order_ == LSBFIRST; settings_valid_ = true;
    }
}
void SPIClass::endTransaction() { transaction_ = false; }
uint8_t SPIClass::transfer(uint8_t data)
{
    uint8_t received = 0;
    if (!settings_valid_ && !api_error_) { api_error_ = UC3B_SPI_NOT_STARTED; }
    if (!api_error_) { (void)uc3b_spi_transfer(data, &received); }
    return received;
}
uint16_t SPIClass::transfer16(uint16_t data)
{
    uint8_t first = transfer((uint8_t)(lsb_ ? data : data >> 8));
    if (lastError()) { return 0; }
    uint8_t second = transfer((uint8_t)(lsb_ ? data >> 8 : data));
    if (lastError()) { return 0; }
    return lsb_ ? (uint16_t)(first | ((uint16_t)second << 8))
                : (uint16_t)(((uint16_t)first << 8) | second);
}
void SPIClass::transfer(void *buffer, size_t count)
{
    uint8_t *bytes = (uint8_t *)buffer;
    if (!count) { return; }
    if (!buffer) { api_error_ = UC3B_SPI_ARGUMENT; return; }
    for (size_t i = 0; i < count; ++i) {
        uint8_t received = transfer(bytes[i]);
        if (lastError()) { return; } /* Retain unread suffix on failure. */
        bytes[i] = received;
    }
}
uint8_t SPIClass::lastError() const { return api_error_ ? api_error_ : uc3b_spi_error(); }
void SPIClass::clearError() { api_error_ = UC3B_SPI_OK; uc3b_spi_clear_error(); }
uint32_t SPIClass::clockHz() const { return settings_valid_ ? uc3b_spi_clock_hz() : 0; }
bool SPIClass::setLoopback(bool enabled)
{
    if (transaction_) { api_error_ = UC3B_SPI_ARGUMENT; return false; }
    return uc3b_spi_loopback(enabled);
}
SPIClass::operator bool() const { return settings_valid_ && uc3b_spi_active() && !lastError(); }
