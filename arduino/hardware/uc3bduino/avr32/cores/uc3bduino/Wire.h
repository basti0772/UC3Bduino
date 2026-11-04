/* SPDX-License-Identifier: MIT */
#ifndef UC3BDUINO_WIRE_H
#define UC3BDUINO_WIRE_H
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#define WIRE_BUFFER_LENGTH 32
/* Foreground single-master subset. GPIO software I2C, not native TWI or Stream.
 * Use the global Wire object; no slave callbacks/global constructors.
 */
class TwoWire {
public:
    void begin();
    void end();
    void setClock(uint32_t hz);
    void beginTransmission(uint8_t address);
    size_t write(uint8_t value);
    size_t write(int value) { return write((uint8_t)value); }
    size_t write(unsigned int value) { return write((uint8_t)value); }
    size_t write(long value) { return write((uint8_t)value); }
    size_t write(unsigned long value) { return write((uint8_t)value); }
    size_t write(const uint8_t *data, size_t count);
    size_t write(const char *text);
    uint8_t endTransmission(bool sendStop = true);
    uint8_t requestFrom(uint8_t address, size_t count, bool sendStop = true);
    int available();
    int read();
    int peek();
    void flush(); /* Synchronous transfers: no queued hardware output to wait for. */
    void setWireTimeout(uint32_t timeout = 25000UL);
    bool getWireTimeoutFlag();
    void clearWireTimeoutFlag();
    uint8_t lastError() const;
    operator bool() const;
private:
    uint8_t tx_[WIRE_BUFFER_LENGTH], rx_[WIRE_BUFFER_LENGTH];
    uint8_t tx_size_, rx_size_, rx_index_, address_, error_;
    bool transmitting_, overflow_;
};
extern TwoWire Wire;
#endif
