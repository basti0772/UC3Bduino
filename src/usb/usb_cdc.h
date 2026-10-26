/* SPDX-License-Identifier: MIT */
#ifndef USB_CDC_H
#define USB_CDC_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
bool usb_cdc_begin(void);
void usb_cdc_end(void);
bool usb_cdc_ready(void);
int usb_cdc_available(void);
int usb_cdc_read(void);
int usb_cdc_peek(void);
bool usb_cdc_write(uint8_t value);
void usb_cdc_flush(void);

/* Private core/driver entry points; caller must serialize them. */
void usb_cdc_reset(void);
void usb_cdc_control_lines(uint16_t value);
void usb_cdc_get_line_coding(uint8_t data[7]);
bool usb_cdc_valid_line_coding(const uint8_t data[7]);
void usb_cdc_set_line_coding(const uint8_t data[7]);
bool usb_cdc_can_receive(uint16_t size);
void usb_cdc_receive(const uint8_t *data, uint16_t size);
void usb_cdc_in_complete(uint8_t ep);
void usb_cdc_halt_changed(uint8_t ep);
void usb_cdc_sof(void);
#ifdef __cplusplus
}
#endif
#endif
