/* SPDX-License-Identifier: MIT */
#ifndef USB_BOOT_TOUCH_H
#define USB_BOOT_TOUCH_H
#include <stdint.h>
#include <stdbool.h>
void usb_boot_touch_reset(void);
void usb_boot_touch_setup(void);
void usb_boot_touch_baud(uint32_t baud);
void usb_boot_touch_lines(uint16_t previous, uint16_t value);
void usb_boot_touch_status_complete(void);
bool usb_boot_touch_frame(void);
#endif
