/* SPDX-License-Identifier: MIT */
#ifndef UC3BDUINO_SPI_H
#define UC3BDUINO_SPI_H
#include <Arduino.h>
#include <stddef.h>
#include "hal/uc3b_spi.h"
#ifndef LSBFIRST
#define LSBFIRST 0
#define MSBFIRST 1
#endif
#define SPI_MODE0 0x00
#define SPI_MODE1 0x04
#define SPI_MODE2 0x08
#define SPI_MODE3 0x0c

/* Use local settings objects: this core does not run global constructors. */
class SPISettings {
public:
    SPISettings(uint32_t clock = 4000000UL, uint8_t bitOrder = MSBFIRST,
                uint8_t dataMode = SPI_MODE0)
        : clock_(clock), order_(bitOrder), mode_(dataMode) { }
private:
    uint32_t clock_;
    uint8_t order_, mode_;
    friend class SPIClass;
};

/* One bus/object; foreground only, transactions cannot nest. No usingInterrupt
 * support, so SPI_HAS_TRANSACTION is deliberately not advertised. USB/timer
 * interrupts stay enabled. Chip select is always controlled by the sketch.
 */
class SPIClass {
public:
    void begin();
    void end();
    void beginTransaction(SPISettings settings);
    void endTransaction();
    uint8_t transfer(uint8_t data);
    uint16_t transfer16(uint16_t data);
    void transfer(void *buffer, size_t count);
    /* UC3Bduino extensions: status, actual clock, and internal diagnostic loopback. */
    uint8_t lastError() const;
    void clearError();
    uint32_t clockHz() const;
    bool setLoopback(bool enabled);
    operator bool() const;
private:
    bool transaction_, lsb_, settings_valid_;
    uint8_t api_error_;
};
extern SPIClass SPI;
#endif
