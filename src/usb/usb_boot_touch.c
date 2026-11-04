/* SPDX-License-Identifier: MIT */
#include "usb_boot_touch.h"
#include "usb_config.h"

static bool touch_armed, touch_pending;
static uint8_t touch_frames;

void usb_boot_touch_reset(void)
{
    touch_armed = touch_pending = false;
    touch_frames = 0;
}
void usb_boot_touch_setup(void)
{
    /* A new SETUP cancels a close request whose status was not acknowledged. */
    touch_pending = false;
}
void usb_boot_touch_baud(uint32_t baud)
{
    (void)baud; /* Some host compilers remove the disabled constant expression. */
    if (!UC3BDUINO_USB_AUTO_UPLOAD || baud != 1200UL) {
        usb_boot_touch_reset();
    } else if (!touch_pending && touch_frames == 0) {
        /* Windows repeats SET_LINE_CODING while changing DTR/closing a port.
         * An identical baud must not erase an acknowledged close countdown.
         */
        touch_armed = true;
    }
}
void usb_boot_touch_lines(uint16_t previous, uint16_t value)
{
    touch_pending = touch_armed && (previous & 1) != 0 && (value & 1) == 0;
    if (value & 1) { touch_frames = 0; }
}
void usb_boot_touch_status_complete(void)
{
    if (touch_pending) {
        touch_pending = touch_armed = false;
        touch_frames = 20; /* Allow 20 full-speed frames after the status ACK. */
    }
}
bool usb_boot_touch_frame(void)
{
    if (touch_frames == 0) { return false; }
    return --touch_frames == 0; /* Consume once, even if the reset backend refuses. */
}
