/* SPDX-License-Identifier: MIT */
#ifndef UC3B_I2C_H
#define UC3B_I2C_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Wire-compatible result codes. GPIO open-drain, single foreground master. */
enum { UC3B_I2C_OK = 0, UC3B_I2C_OVERFLOW = 1, UC3B_I2C_ADDRESS_NACK = 2,
       UC3B_I2C_DATA_NACK = 3, UC3B_I2C_OTHER = 4, UC3B_I2C_TIMEOUT = 5 };
bool uc3b_i2c_begin(void);
void uc3b_i2c_end(void);
bool uc3b_i2c_set_clock(uint32_t hz);
bool uc3b_i2c_set_timeout(uint32_t us);
uint8_t uc3b_i2c_write(uint8_t address, const uint8_t *data, size_t count, bool stop);
uint8_t uc3b_i2c_read(uint8_t address, uint8_t *data, size_t count, bool stop);
bool uc3b_i2c_timeout_flag(void);
void uc3b_i2c_clear_timeout(void);
bool uc3b_i2c_active(void);
#ifdef __cplusplus
}
#endif
#endif
