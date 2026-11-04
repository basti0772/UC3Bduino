/* SPDX-License-Identifier: MIT */
/* Exercise the real EP0/class state machines, replacing only the transport. */
#define main protocol_regression_main
#include "usb_protocol_test.c"
#undef main

static void frames(unsigned count)
{
    while (count--) { usb_cdc_sof(); }
}
static void baud(uint32_t value)
{
    uint8_t coding[7] = {0, 0, 0, 0, 0, 0, 8};
    coding[0] = (uint8_t)value; coding[1] = (uint8_t)(value >> 8);
    coding[2] = (uint8_t)(value >> 16); coding[3] = (uint8_t)(value >> 24);
    setup(0x21, 0x20, 0, 0, 7); usb_device_control_out(coding, 7); finish_status();
}
static void lines(uint16_t value)
{
    setup(0x21, 0x22, value, 0, 0); finish_status();
}
static void fresh(void)
{
    usb_device_reset(); configure(); boot_requests = 0;
}
int main(void)
{
    assert(protocol_regression_main() == 0);
    assert(usb_cdc_begin()); fresh();
    baud(1200); frames(100); assert(boot_requests == 0); /* baud alone */
    lines(1); frames(100); assert(boot_requests == 0); /* DTR still high */
    setup(0x21, 0x22, 0, 0, 0);
    frames(100); assert(boot_requests == 0); /* no status sent */
    usb_device_control_in_ready(); frames(100); assert(boot_requests == 0);
    usb_device_control_in_ready(); /* host ACK */
    frames(19); assert(boot_requests == 0);
    frames(1); assert(boot_requests == UC3BDUINO_USB_AUTO_UPLOAD);
    frames(100); assert(boot_requests == UC3BDUINO_USB_AUTO_UPLOAD); /* consumed */
    fresh(); baud(1200); lines(0); frames(10);
    baud(1200); baud(1200); /* Windows repeats coding around DTR changes. */
    frames(9); assert(boot_requests == 0);
    frames(1); assert(boot_requests == UC3BDUINO_USB_AUTO_UPLOAD);
    frames(100); assert(boot_requests == UC3BDUINO_USB_AUTO_UPLOAD);
    fresh(); baud(9600); lines(0); frames(100); assert(boot_requests == 0);
    fresh(); baud(1200); baud(115200); lines(0); frames(100); assert(boot_requests == 0);
    fresh(); lines(0); baud(1200); lines(0); frames(100); assert(boot_requests == 0); /* no edge */
    fresh(); baud(1200); lines(2); frames(20); /* RTS high, DTR low */
    assert(boot_requests == UC3BDUINO_USB_AUTO_UPLOAD);
    fresh(); baud(1200); setup(0x21, 0x22, 0, 0, 0); usb_device_control_in_ready();
    setup(0xa1, 0x21, 0, 0, 7); read_response(); /* aborted/hidden status */
    frames(100); assert(boot_requests == 0);
    fresh(); baud(1200); lines(0); frames(10); lines(1); frames(100); assert(boot_requests == 0);
    fresh(); baud(1200); lines(0); frames(10); baud(9600); frames(100); assert(boot_requests == 0);
    fresh(); baud(1200); lines(0); frames(10); usb_device_reset(); configure();
    frames(100); assert(boot_requests == 0);
    fresh(); baud(1200); lines(0); frames(10); usb_device_suspend(true); usb_device_suspend(false);
    frames(100); assert(boot_requests == 0);
    fresh(); baud(1200); lines(0); setup(0, 9, 1, 0, 0); finish_status();
    frames(100); assert(boot_requests == 0);
    fresh(); setup(0x21, 0x20, 0, 0, 6); assert(stalled);
    lines(0); frames(100); assert(boot_requests == 0);
    usb_cdc_end(); assert(locks == 0);
    puts("PASS: touch enabled/disabled, baud, DTR edge, status ACK, delay, one-shot, cancellation, reset and suspend");
    return 0;
}
