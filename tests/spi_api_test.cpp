/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <SPI.h>
static bool active, order, loopback;
static uint8_t mode, error_code, sent[32];
static uint32_t clock_hz;
static unsigned int writes, fail_at;
bool uc3b_spi_begin(void) { active = true; error_code = 0; return true; }
void uc3b_spi_end(void) { active = false; }
bool uc3b_spi_active(void) { return active; }
uint8_t uc3b_spi_error(void) { return error_code; }
void uc3b_spi_clear_error(void) { error_code = 0; }
uint32_t uc3b_spi_clock_hz(void) { return clock_hz; }
bool uc3b_spi_loopback(bool enabled) { loopback = enabled; return active; }
bool uc3b_spi_configure(uint32_t hz, bool lsb, uint8_t data_mode)
{
    if (!active) { error_code = UC3B_SPI_NOT_STARTED; return false; }
    clock_hz = hz; order = lsb; mode = data_mode; error_code = 0; return true;
}
bool uc3b_spi_transfer(uint8_t tx, uint8_t *rx)
{
    if (!active || error_code) { return false; }
    assert(writes < sizeof(sent)); sent[writes++] = tx;
    if (fail_at && writes == fail_at) { error_code = UC3B_SPI_TIMEOUT; return false; }
    *rx = (uint8_t)(tx ^ 0x12); return true;
}
#include "../arduino/hardware/uc3bduino/avr32/cores/uc3bduino/SPI.cpp"
int main(void)
{
    SPI.beginTransaction(SPISettings());
    assert(SPI.lastError() == UC3B_SPI_NOT_STARTED); SPI.endTransaction();
    SPI.begin(); assert(SPI);
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
    assert(!order && mode == 3 && SPI.clockHz() == 1000000);
    assert(SPI.transfer16(0x96e1) == 0x84f3 && sent[0] == 0x96 && sent[1] == 0xe1);
    SPI.endTransaction();
    SPI.beginTransaction(SPISettings(100000, LSBFIRST, SPI_MODE1));
    assert(order && mode == 1);
    assert(SPI.transfer16(0x96e1) == 0x84f3 && sent[2] == 0xe1 && sent[3] == 0x96);
    uint8_t buffer[] = { 0, 0x55, 0x80 };
    SPI.transfer(buffer, sizeof(buffer));
    assert(buffer[0] == 0x12 && buffer[1] == 0x47 && buffer[2] == 0x92);
    unsigned int before = writes;
    SPI.transfer(0, 0); assert(writes == before && SPI);
    SPI.transfer(0, 1); assert(SPI.lastError() == UC3B_SPI_ARGUMENT && writes == before);
    SPI.endTransaction();
    SPI.beginTransaction(SPISettings(1, 7, SPI_MODE0));
    SPI.transfer((uint8_t)0); assert(writes == before && SPI.lastError());
    SPI.clearError(); SPI.transfer((uint8_t)0);
    assert(writes == before && !SPI && SPI.clockHz() == 0); SPI.endTransaction();
    SPI.begin(); assert(SPI);
    SPI.beginTransaction(SPISettings(1, MSBFIRST, 1));
    SPI.transfer((uint8_t)0); assert(writes == before && SPI.lastError()); SPI.endTransaction();
    SPI.beginTransaction(SPISettings());
    SPI.beginTransaction(SPISettings(100000));
    SPI.transfer((uint8_t)0); assert(writes == before && SPI.lastError()); SPI.endTransaction();
    SPI.beginTransaction(SPISettings());
    fail_at = writes + 2;
    buffer[0] = 0; buffer[1] = 0x55; buffer[2] = 0x80;
    SPI.transfer(buffer, sizeof(buffer));
    assert(writes == fail_at && buffer[0] == 0x12 && buffer[1] == 0x55 && buffer[2] == 0x80);
    assert(SPI.lastError() == UC3B_SPI_TIMEOUT); SPI.endTransaction();
    SPI.clearError(); fail_at = 0;
    assert(SPI.setLoopback(true) && loopback);
    SPI.end(); assert(!SPI); SPI.begin(); assert(SPI); SPI.end();
    puts("PASS: SPI API defaults, settings, endian-independent word order, in-place buffers, invalid/nested transactions and partial failure");
    return 0;
}
