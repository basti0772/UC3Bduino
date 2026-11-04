/* SPDX-License-Identifier: MIT */
#ifndef UC3B_USB_DEVICE_H
#define UC3B_USB_DEVICE_H
#include <stdint.h>
#include <stdbool.h>

/* Private transport boundary. Core/CDC callbacks run in USB ISR or with GM
 * masked; only begin/end and the Arduino-facing CDC calls run in foreground.
 */
bool usb_hw_start(void);
void usb_hw_stop(void);
void usb_hw_request_bootloader(void);
bool usb_hw_configure_data(bool enable);
void usb_hw_set_address(uint8_t address);
void usb_hw_control_stall(void);
void usb_hw_control_in_irq(bool enable);
void usb_hw_control_send(const uint8_t *data, uint16_t size);
bool usb_hw_halted(uint8_t ep);
void usb_hw_halt(uint8_t ep, bool halt);
bool usb_hw_send(uint8_t ep, const uint8_t *data, uint16_t size);
void usb_hw_out_irq(bool enable);
uint32_t usb_hw_lock(void);
void usb_hw_unlock(uint32_t key);
bool usb_hw_foreground(void);
uint32_t usb_hw_millis(void);
#endif
