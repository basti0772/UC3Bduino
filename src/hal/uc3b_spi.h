/* SPDX-License-Identifier: MIT */
#ifndef UC3B_SPI_H
#define UC3B_SPI_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
    UC3B_SPI_OK = 0, UC3B_SPI_NOT_STARTED, UC3B_SPI_ARGUMENT,
    UC3B_SPI_CONTEXT, UC3B_SPI_TIMEOUT, UC3B_SPI_HARDWARE
};
/* Single master, foreground only. No ISR, DMA or automatic chip select.
 * PA14=MOSI/A, PA25=MISO/A, PA15=SCK/A; caller owns external chip selects.
 */
bool uc3b_spi_begin(void);
void uc3b_spi_end(void);
bool uc3b_spi_configure(uint32_t hz, bool lsb_first, uint8_t mode);
bool uc3b_spi_transfer(uint8_t tx, uint8_t *rx);
bool uc3b_spi_loopback(bool enabled);
bool uc3b_spi_active(void);
uint32_t uc3b_spi_clock_hz(void);
uint8_t uc3b_spi_error(void);
void uc3b_spi_clear_error(void);
#ifdef __cplusplus
}
#endif
#endif
