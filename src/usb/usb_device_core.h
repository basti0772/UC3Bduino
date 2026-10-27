/* SPDX-License-Identifier: MIT */
#ifndef USB_DEVICE_CORE_H
#define USB_DEVICE_CORE_H
#include <stdint.h>
#include <stdbool.h>

bool usb_device_begin(void);
void usb_device_end(void);
/* Events from the hardware adapter, serialized with foreground calls. */
void usb_device_reset(void);
void usb_device_suspend(bool suspended);
void usb_device_setup(const uint8_t setup[8]);
void usb_device_control_out(const uint8_t *data, uint16_t size);
void usb_device_control_in_ready(void);
bool usb_device_configured(void);
#endif
