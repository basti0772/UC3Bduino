/* SPDX-License-Identifier: MIT */
#ifndef UC3BDUINO_USB_CONFIG_H
#define UC3BDUINO_USB_CONFIG_H

/* Explicit opt-in: requires a separately selected, verified bootloader backend.
 * Default off preserves ordinary 1200-baud CDC use and never changes boot fuses.
 */
#ifndef UC3BDUINO_USB_AUTO_UPLOAD
#define UC3BDUINO_USB_AUTO_UPLOAD 0
#endif
#if UC3BDUINO_USB_AUTO_UPLOAD != 0 && UC3BDUINO_USB_AUTO_UPLOAD != 1
#error "UC3BDUINO_USB_AUTO_UPLOAD must be 0 or 1"
#endif

/* USB VID/PID must be configured by the developer.
 * Zero means unconfigured: begin() fails without enabling USB or attaching.
 * Supply only IDs you are authorized to use, in compiler flags or here.
 */
#ifndef UC3BDUINO_USB_VENDOR_ID
#define UC3BDUINO_USB_VENDOR_ID 0
#endif
#ifndef UC3BDUINO_USB_PRODUCT_ID
#define UC3BDUINO_USB_PRODUCT_ID 0
#endif
#if UC3BDUINO_USB_VENDOR_ID < 0 || UC3BDUINO_USB_VENDOR_ID > 65535 || \
    UC3BDUINO_USB_PRODUCT_ID < 0 || UC3BDUINO_USB_PRODUCT_ID > 65535
#error "USB VID/PID must fit in 16 bits"
#endif
/* ASCII strings, at most 31 characters each (converted to UTF-16LE).
 * Leave serial empty unless you can supply a stable, unique per-device value.
 */
#ifndef UC3BDUINO_USB_MANUFACTURER
#define UC3BDUINO_USB_MANUFACTURER "UC3Bduino"
#endif
#ifndef UC3BDUINO_USB_PRODUCT
#define UC3BDUINO_USB_PRODUCT "UC3Bduino CDC"
#endif
#ifndef UC3BDUINO_USB_SERIAL_STRING
#define UC3BDUINO_USB_SERIAL_STRING ""
#endif
/* No remote wakeup. Board must satisfy the declared power budget.
 * Suspend keeps CPU/USB clocks running: bus-powered USB power compliance
 * needs a later board-specific low-power implementation; see USB_CDC.md.
 */
#ifndef UC3BDUINO_USB_SELF_POWERED
#define UC3BDUINO_USB_SELF_POWERED 0
#endif
#ifndef UC3BDUINO_USB_MAX_POWER_MA
#define UC3BDUINO_USB_MAX_POWER_MA 100
#endif
#if (UC3BDUINO_USB_SELF_POWERED != 0 && UC3BDUINO_USB_SELF_POWERED != 1) || \
    UC3BDUINO_USB_MAX_POWER_MA < 0 || UC3BDUINO_USB_MAX_POWER_MA > 500 || \
    (UC3BDUINO_USB_MAX_POWER_MA % 2) != 0
#error "Invalid USB power descriptor configuration"
#endif
#ifndef USB_SERIAL_RX_BUFFER_SIZE
#define USB_SERIAL_RX_BUFFER_SIZE 128
#endif
#ifndef USB_SERIAL_TX_BUFFER_SIZE
#define USB_SERIAL_TX_BUFFER_SIZE 128
#endif
#if USB_SERIAL_RX_BUFFER_SIZE < 128 || USB_SERIAL_RX_BUFFER_SIZE > 1024 || \
    (USB_SERIAL_RX_BUFFER_SIZE & (USB_SERIAL_RX_BUFFER_SIZE - 1)) != 0 || \
    USB_SERIAL_TX_BUFFER_SIZE < 128 || USB_SERIAL_TX_BUFFER_SIZE > 1024 || \
    (USB_SERIAL_TX_BUFFER_SIZE & (USB_SERIAL_TX_BUFFER_SIZE - 1)) != 0
#error "USB rings must be powers of two, 128..1024 (one slot is reserved)"
#endif
#define USB_EP_CONTROL 0
#define USB_EP_NOTIFY 1
#define USB_EP_OUT 2
#define USB_EP_IN 3
#define USB_EP_CONTROL_SIZE 64
#define USB_EP_NOTIFY_SIZE 16
#define USB_EP_DATA_SIZE 64
#define USB_CDC_COMM_INTERFACE 0
#define USB_CDC_DATA_INTERFACE 1
#define USB_CDC_FLUSH_TIMEOUT_MS 100UL
#endif
