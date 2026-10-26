/* SPDX-License-Identifier: MIT
 * Host-only simulated transport. Synthetic 0xffff identity is NEVER built into
 * firmware, offered as a usable VID/PID, or sent to a real USB controller.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef TEST_UNCONFIGURED
#define UC3BDUINO_USB_VENDOR_ID 65535
#define UC3BDUINO_USB_PRODUCT_ID 65535
#endif
#define UC3BDUINO_USB_MANUFACTURER "123456789012345678901234567890A"
#include "../src/usb/usb_config.h"
#include "../src/usb/uc3b_usb_device.h"
#include "../src/usb/usb_cdc.h"
#include "../src/usb/usb_device_core.h"

static uint8_t control_data[512], in_data[2048], notify_data[16];
static uint16_t control_sizes[16], in_sizes[128], control_used, in_used;
static unsigned control_packets, in_packets, notify_packets, starts, stops, address_writes;
static bool control_irq, stalled, hw_started, endpoints, fail_allocation, fail_start;
static bool halted[4], busy[4], foreground = true, out_irq;
static uint8_t hw_address;
static uint32_t ticks, locks;
static bool auto_ack;

bool usb_hw_start(void) { ++starts; hw_started = !fail_start; return hw_started; }
void usb_hw_stop(void) { ++stops; hw_started = false; }
bool usb_hw_configure_data(bool enable)
{
    unsigned i;
    endpoints = enable && !fail_allocation;
    for (i = 1; i < 4; ++i) { halted[i] = busy[i] = false; }
    out_irq = endpoints;
    return !enable || !fail_allocation;
}
void usb_hw_set_address(uint8_t value) { hw_address = value; ++address_writes; }
void usb_hw_control_stall(void) { stalled = true; }
void usb_hw_control_in_irq(bool enable) { control_irq = enable; }
void usb_hw_control_send(const uint8_t *data, uint16_t size)
{
    assert(size <= 64 && control_packets < 16 && control_used + size <= sizeof(control_data));
    control_sizes[control_packets++] = size;
    if (size) { memcpy(control_data + control_used, data, size); }
    control_used += size;
}
bool usb_hw_halted(uint8_t ep) { return halted[ep]; }
void usb_hw_halt(uint8_t ep, bool halt) { halted[ep] = halt; busy[ep] = false; }
bool usb_hw_send(uint8_t ep, const uint8_t *data, uint16_t size)
{
    assert(endpoints && !halted[ep] && ep != USB_EP_OUT);
    if (busy[ep]) { return false; }
    busy[ep] = true;
    if (ep == USB_EP_NOTIFY) { assert(size == 10); memcpy(notify_data, data, size); ++notify_packets; }
    else {
        assert(size <= 64 && in_packets < 128 && in_used + size <= sizeof(in_data));
        in_sizes[in_packets++] = size;
        if (size) { memcpy(in_data + in_used, data, size); }
        in_used += size;
    }
    return true;
}
void usb_hw_out_irq(bool enable) { out_irq = enable; }
uint32_t usb_hw_lock(void) { return locks++; }
void usb_hw_unlock(uint32_t key) { assert(locks == key + 1); --locks; }
bool usb_hw_foreground(void) { return foreground; }
static void acknowledge(uint8_t ep)
{
    assert(busy[ep]); busy[ep] = false; usb_cdc_in_complete(ep);
}
uint32_t usb_hw_millis(void)
{
    assert(locks == 0);
    if (auto_ack) {
        if (busy[USB_EP_IN]) { acknowledge(USB_EP_IN); }
        usb_cdc_sof();
    }
    return ticks++;
}

/* Compile the real portable sources, with only the register adapter replaced. */
#include "../src/usb/usb_device_core.c"
#include "../src/usb/usb_cdc.c"

static void setup(uint8_t type, uint8_t request, uint16_t value, uint16_t index, uint16_t length)
{
    uint8_t data[8];
    data[0] = type; data[1] = request;
    data[2] = (uint8_t)value; data[3] = (uint8_t)(value >> 8);
    data[4] = (uint8_t)index; data[5] = (uint8_t)(index >> 8);
    data[6] = (uint8_t)length; data[7] = (uint8_t)(length >> 8);
    stalled = false; control_packets = control_used = 0;
    usb_device_setup(data);
}
static void finish_status(void)
{
    assert(!stalled && control_irq);
    usb_device_control_in_ready();
    assert(control_packets == 1 && control_sizes[0] == 0 && control_irq);
    usb_device_control_in_ready();
    assert(!control_irq);
}
static void read_response(void)
{
    unsigned steps = 0;
    while (control_irq) { assert(++steps < 10); usb_device_control_in_ready(); }
    usb_device_control_out(0, 0);
    assert(!stalled);
}
static void set_address(void)
{
    setup(0, 5, 7, 0, 0);
    finish_status(); assert(hw_address == 7);
}
static void configure(void)
{
    set_address(); setup(0, 9, 1, 0, 0); finish_status();
    assert(endpoints && usb_device_configured() && !usb_cdc_ready());
    setup(0x21, 0x22, 3, 0, 0); finish_status(); assert(usb_cdc_ready());
}

int main(void)
{
#ifdef TEST_UNCONFIGURED
    assert(!usb_cdc_begin() && starts == 0 && !usb_cdc_ready());
    puts("PASS: missing USB identity never starts hardware");
    return 0;
#else
    uint8_t data[64], coding[7] = {0x80, 0x25, 0, 0, 2, 2, 7};
    unsigned i, n;
    uint32_t before;
    memset(data, 0xa5, sizeof(data));
    assert(!usb_cdc_ready() && usb_cdc_read() == -1 && !usb_cdc_write(1));
    foreground = false; assert(!usb_cdc_begin() && starts == 0); foreground = true;
    fail_start = true; assert(!usb_cdc_begin()); fail_start = false; assert(usb_cdc_begin());
    setup(0x80, 6, 0x0100, 0, 8); read_response();
    assert(control_used == 8 && control_data[0] == 18 && control_data[4] == 2 && control_data[5] == 2);
    setup(0x80, 6, 0x0100, 0, 255); read_response();
    assert(control_used == 18 && control_data[16] == 0 && control_packets == 1);
    setup(0x80, 6, 0x0200, 0, 255); read_response();
    assert(control_used == 67 && control_packets == 2 && control_sizes[0] == 64 && control_sizes[1] == 3);
    assert(control_data[2] == 67 && control_data[4] == 2 && control_data[7] == 0x80);
    assert(control_data[39] == 0x81 && control_data[55] == 2 && control_data[65] == 0);
    setup(0x80, 6, 0x0300, 0, 255); read_response();
    assert(control_used == 4 && control_data[2] == 9 && control_data[3] == 4);
    setup(0x80, 6, 0x0301, 0x409, 255); read_response();
    assert(control_used == 64 && control_packets == 2 && control_sizes[1] == 0);
    assert(control_data[2] == '1' && control_data[3] == 0);
    setup(0x80, 6, 0x0301, 0x409, 64); read_response(); assert(control_packets == 1);
    setup(0x80, 6, 0x0100, 0, 0); finish_status(); /* no data -> IN status */
    setup(0x80, 6, 0x0600, 0, 10); assert(stalled); /* no high-speed qualifier */
    setup(0x80, 6, 0x0303, 0x409, 64); assert(stalled); /* absent serial */
    setup(0x80, 6, 0x0200, 0, 255); usb_device_control_in_ready();
    usb_device_control_out(0, 0); assert(!control_irq && !stalled); /* early status */
    setup(0x80, 6, 0x0200, 0, 255); usb_device_control_in_ready();
    setup(0x80, 6, 0x0100, 0, 18); read_response(); assert(control_used == 18); /* abort */
    before = address_writes;
    setup(0, 5, 42, 0, 0); usb_device_control_in_ready(); assert(address_writes == before);
    setup(0x80, 6, 0x0100, 0, 18); read_response(); assert(address_writes == before);
    set_address(); assert(address_writes == before + 1);
    setup(0, 5, 0, 0, 0); finish_status(); assert(hw_address == 0);
    setup(0, 9, 1, 0, 0); assert(stalled); /* default state cannot configure */
    set_address(); fail_allocation = true;
    setup(0, 9, 1, 0, 0); assert(stalled && !usb_device_configured()); fail_allocation = false;
    configure();
    setup(0, 5, 8, 0, 0); assert(stalled); /* configured address must not change */
    setup(0x80, 8, 0, 0, 1); read_response(); assert(control_data[0] == 1);
    setup(0xa1, 0x21, 0, 0, 7); read_response(); assert(control_used == 7 && control_data[1] == 0xc2);
    setup(0x21, 0x20, 0, 0, 7); usb_device_control_out(coding, 7);
    finish_status(); usb_cdc_get_line_coding(data); assert(memcmp(coding, data, 7) == 0);
    setup(0x21, 0x20, 0, 0, 7); usb_device_control_out(coding, 7); usb_device_control_in_ready();
    setup(0xa1, 0x21, 0, 0, 7); read_response(); /* SETUP hides previous status completion */
    assert(memcmp(coding, control_data, 7) == 0);
    coding[6] = 9;
    setup(0x21, 0x20, 0, 0, 7); usb_device_control_out(coding, 7); assert(stalled);
    setup(0x21, 0x20, 0, 0, 7); usb_device_control_out(coding, 6); assert(stalled);
    setup(0x21, 0x20, 0, 1, 7); assert(stalled);
    setup(0x21, 0x22, 4, 0, 0); assert(stalled);
    setup(0x21, 0x23, 0, 0, 0); assert(stalled); /* SEND_BREAK not advertised */
    setup(0, 3, 1, 0, 0); assert(stalled); /* remote wake not advertised */
    setup(0x82, 0, 0, 0x82, 2); assert(stalled); /* wrong endpoint direction */
    setup(2, 3, 0, 0x83, 0); finish_status(); assert(halted[3]);
    setup(0x82, 0, 0, 0x83, 2); read_response(); assert(control_data[0] == 1);
    setup(2, 1, 0, 0x83, 0); finish_status(); assert(!halted[3]);
    setup(1, 11, 1, 1, 0); assert(stalled); /* no alternate 1 */
    setup(0x81, 10, 0, 1, 1); read_response(); assert(control_data[0] == 0);
    usb_cdc_sof(); assert(notify_packets == 1 && notify_data[8] == 3);
    acknowledge(1); usb_cdc_sof(); assert(notify_packets == 1); /* no duplicate notification */
    setup(0x21, 0x22, 0, 0, 0); finish_status();
    assert(!usb_cdc_ready() && !usb_cdc_write('x'));
    usb_cdc_sof(); assert(notify_packets == 2 && notify_data[8] == 0); acknowledge(1);
    setup(0x21, 0x22, 1, 0, 0); finish_status();
    for (i = 0; i < USB_SERIAL_TX_BUFFER_SIZE - 1; ++i) { assert(usb_cdc_write((uint8_t)i)); }
    assert(!usb_cdc_write(1) && in_packets == 0); /* buffered, no per-character packet */
    usb_cdc_sof(); assert(in_packets == 1 && in_sizes[0] == 64);
    usb_cdc_sof(); assert(in_packets == 1); /* blocked until host ACK */
    acknowledge(3); usb_cdc_sof(); assert(in_sizes[1] == 63); acknowledge(3);
    for (i = 0; i < 127; ++i) { assert(in_data[i] == (uint8_t)i); }
    for (i = 0; i < 64; ++i) { assert(usb_cdc_write((uint8_t)i)); }
    usb_cdc_sof(); acknowledge(3); usb_cdc_sof(); assert(in_sizes[in_packets - 1] == 0); acknowledge(3);
    /* Ring wrap and complete-packet admission. */
    assert(usb_cdc_can_receive(64)); usb_cdc_receive(data, 64);
    assert(!usb_cdc_can_receive(64)); out_irq = false;
    assert(usb_cdc_peek() == data[0] && usb_cdc_available() == 64);
    assert(usb_cdc_read() == data[0] && out_irq && usb_cdc_available() == 63);
    usb_cdc_receive(data, 64); assert(usb_cdc_available() == 127 && !usb_cdc_can_receive(1));
    for (i = 0; i < 127; ++i) { assert(usb_cdc_read() >= 0); }
    assert(usb_cdc_read() == -1 && usb_cdc_peek() == -1);
    usb_cdc_receive(data, 2);
    assert(usb_cdc_write('F')); before = ticks; usb_cdc_flush();
    assert(ticks - before <= 102 && busy[3] && usb_cdc_available() == 2); /* stalled host timeout */
    auto_ack = true; usb_cdc_flush(); auto_ack = false; assert(!busy[3] && usb_cdc_available() == 2);
    foreground = false; before = ticks; usb_cdc_flush(); assert(ticks == before); foreground = true;
    usb_device_suspend(true); assert(!usb_cdc_ready() && !usb_cdc_write('S'));
    before = ticks; usb_cdc_flush(); assert(ticks - before <= 2);
    usb_device_suspend(false); assert(usb_cdc_ready());
    setup(0, 9, 0, 0, 0); finish_status();
    assert(!usb_cdc_ready() && !endpoints && usb_cdc_available() == 0);
    configure(); usb_cdc_receive(data, 4); assert(usb_cdc_write('R'));
    setup(0, 5, 1, 0, 0); assert(stalled);
    usb_device_reset(); (void)usb_hw_configure_data(false); /* bus-reset adapter side */
    assert(!usb_cdc_ready() && usb_cdc_available() == 0 && !usb_cdc_write('x'));
    n = in_packets; configure(); usb_cdc_sof(); assert(in_packets == n); /* old TX discarded */
    usb_cdc_end(); assert(!hw_started && stops == 1 && !usb_cdc_ready());
    assert(usb_cdc_begin()); configure(); assert(usb_cdc_ready()); usb_cdc_end();
    assert(locks == 0);
    puts("PASS: USB descriptors/EP0 phases/ZLP/aborts/address/config/class requests/halts/rings/backpressure/flush/reset/suspend/reconnect");
    return 0;
#endif
}
