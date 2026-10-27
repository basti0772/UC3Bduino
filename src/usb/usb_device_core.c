/* SPDX-License-Identifier: MIT
 * Original implementation from USB 2.0 ch. 9, CDC 1.2 and PSTN 1.2.
 * Wire data is explicitly little-endian; AVR32 UC is big-endian.
 */
#include "usb_device_core.h"
#include "uc3b_usb_device.h"
#include "usb_cdc.h"
#include "usb_config.h"

#define LO(v) ((v) & 255)
#define HI(v) (((v) >> 8) & 255)
static const uint8_t device_descriptor[] = {
    18, 1, 0, 2, 2, 2, 0, USB_EP_CONTROL_SIZE,
    LO(UC3BDUINO_USB_VENDOR_ID), HI(UC3BDUINO_USB_VENDOR_ID),
    LO(UC3BDUINO_USB_PRODUCT_ID), HI(UC3BDUINO_USB_PRODUCT_ID),
    0, 1, 1, 2, sizeof(UC3BDUINO_USB_SERIAL_STRING) > 1 ? 3 : 0, 1
};
static const uint8_t configuration_descriptor[] = {
    9, 2, 67, 0, 2, 1, 0, 0x80 | (UC3BDUINO_USB_SELF_POWERED << 6), UC3BDUINO_USB_MAX_POWER_MA / 2,
    9, 4, USB_CDC_COMM_INTERFACE, 0, 1, 2, 2, 0, 0,
    5, 0x24, 0, 0x20, 1, /* Header: CDC 1.20 */
    4, 0x24, 2, 2,       /* ACM: line coding/control lines + SERIAL_STATE */
    5, 0x24, 6, USB_CDC_COMM_INTERFACE, USB_CDC_DATA_INTERFACE, /* Union */
    5, 0x24, 1, 0, USB_CDC_DATA_INTERFACE, /* No device call management */
    7, 5, 0x80 | USB_EP_NOTIFY, 3, USB_EP_NOTIFY_SIZE, 0, 16,
    9, 4, USB_CDC_DATA_INTERFACE, 0, 2, 0x0a, 0, 0, 0,
    7, 5, USB_EP_OUT, 2, USB_EP_DATA_SIZE, 0, 0,
    7, 5, 0x80 | USB_EP_IN, 2, USB_EP_DATA_SIZE, 0, 0
};
typedef char descriptor_sizes[(sizeof(configuration_descriptor) == 67
    && sizeof(UC3BDUINO_USB_MANUFACTURER) <= 32
    && sizeof(UC3BDUINO_USB_PRODUCT) <= 32
    && sizeof(UC3BDUINO_USB_SERIAL_STRING) <= 32) ? 1 : -1];

enum ControlState { IDLE, DATA_IN, DATA_OUT_LINE, STATUS_IN, STATUS_OUT, STALLED };
enum Pending { NONE, ADDRESS };
static enum ControlState state;
static enum Pending pending;
static uint8_t address, configuration;
static bool suspended, active, send_zlp, status_sent;
static uint16_t pending_value, remaining;
static const uint8_t *response;
static uint8_t reply[USB_EP_CONTROL_SIZE];

static uint16_t le16(const uint8_t *data)
{
    return (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
}

static void stall(void)
{
    state = STALLED;
    pending = NONE;
    usb_hw_control_in_irq(false);
    usb_hw_control_stall();
}

static void status_in(void);

static void control_read(const uint8_t *data, uint16_t size, uint16_t requested)
{
    /* USB 2.0 8.5.3: with wLength=0 there is no data stage and status is IN,
     * even if the setup's direction bit is set.
     */
    if (requested == 0) { status_in(); return; }
    response = data;
    remaining = size < requested ? size : requested;
    send_zlp = size < requested && (size % USB_EP_CONTROL_SIZE) == 0;
    state = DATA_IN;
    usb_hw_control_in_irq(true);
}

static void status_in(void)
{
    state = STATUS_IN;
    status_sent = false;
    usb_hw_control_in_irq(true);
}

static bool descriptor(uint16_t value, uint16_t index, uint16_t length)
{
    uint8_t number = (uint8_t)value;
    const char *text;
    uint16_t size;
    if (value == 0x0100 && index == 0) {
        control_read(device_descriptor, sizeof(device_descriptor), length);
    } else if (value == 0x0200 && index == 0) {
        control_read(configuration_descriptor, sizeof(configuration_descriptor), length);
    } else if ((value >> 8) == 3 && number == 0 && index == 0) {
        reply[0] = 4; reply[1] = 3; reply[2] = 9; reply[3] = 4;
        control_read(reply, 4, length); /* English (United States) */
    } else if ((value >> 8) == 3 && index == 0x0409 && number >= 1 && number <= 3) {
        text = number == 1 ? UC3BDUINO_USB_MANUFACTURER :
            (number == 2 ? UC3BDUINO_USB_PRODUCT : UC3BDUINO_USB_SERIAL_STRING);
        if (number == 3 && text[0] == 0) { return false; }
        size = 2;
        while (*text) {
            if ((uint8_t)*text > 127) { return false; } /* ASCII config contract */
            reply[size++] = (uint8_t)*text++;
            reply[size++] = 0;
        }
        reply[0] = (uint8_t)size; reply[1] = 3;
        control_read(reply, size, length);
    } else { return false; } /* Includes qualifier/other-speed: full-speed only. */
    return true;
}

static bool valid_endpoint(uint16_t index)
{
    if (index == 0 || index == 0x80) { return true; }
    return configuration && (index == (0x80 | USB_EP_NOTIFY)
        || index == USB_EP_OUT || index == (0x80 | USB_EP_IN));
}

void usb_device_setup(const uint8_t setup[8])
{
    uint8_t type = setup[0], request = setup[1];
    uint16_t value = le16(setup + 2), index = le16(setup + 4), length = le16(setup + 6);
    /* SETUP aborts the previous transfer, including an unacknowledged status.
     * Hardware has already cleared EP0 STALL and reset its data sequence.
     */
    state = IDLE; pending = NONE; remaining = 0; send_zlp = false;
    usb_hw_control_in_irq(false);
    if (type == 0x80 && request == 6) {
        if (!descriptor(value, index, length)) { stall(); }
        return;
    }
    if (type == 0 && request == 5 && index == 0 && length == 0 && value <= 127 && !configuration) {
        pending = ADDRESS; pending_value = value; status_in(); return;
    }
    if (type == 0 && request == 9 && index == 0 && length == 0 && value <= 1 && address) {
        /* Fixed layout is allocated in ascending order. Refuse ACK if the
         * controller cannot allocate it. Selecting even the same config resets
         * all data endpoints, halts/toggles, class state and queued bytes.
         */
        configuration = 0;
        usb_cdc_reset();
        if (!usb_hw_configure_data(value != 0)) { stall(); return; }
        configuration = (uint8_t)value;
        status_in(); return;
    }
    if (type == 0x80 && request == 8 && value == 0 && index == 0 && length == 1 && address) {
        reply[0] = configuration; control_read(reply, 1, length); return;
    }
    if (request == 0 && value == 0 && length == 2) { /* GET_STATUS */
        reply[0] = 0; reply[1] = 0;
        if (type == 0x80 && index == 0) { reply[0] = UC3BDUINO_USB_SELF_POWERED; }
        else if (type == 0x81 && configuration && index < 2) { /* interface */ }
        else if (type == 0x82 && address && valid_endpoint(index)) {
            reply[0] = (index & 15) ? (uint8_t)usb_hw_halted((uint8_t)(index & 15)) : 0;
        } else { stall(); return; }
        control_read(reply, 2, length); return;
    }
    if (type == 2 && (request == 1 || request == 3) && value == 0 && length == 0
        && configuration && (index & 15) != 0 && valid_endpoint(index)) {
        usb_hw_halt((uint8_t)(index & 15), request == 3);
        usb_cdc_halt_changed((uint8_t)(index & 15));
        status_in(); return;
    }
    if (type == 0x81 && request == 10 && value == 0 && length == 1 && configuration && index < 2) {
        reply[0] = 0; control_read(reply, 1, length); return; /* GET_INTERFACE */
    }
    if (type == 1 && request == 11 && value == 0 && length == 0 && configuration && index < 2) {
        /* One alternate setting. SET_INTERFACE restores this interface's
         * endpoints to DATA0 and unhalted, without affecting the other one.
         */
        uint8_t ep;
        for (ep = index == 0 ? USB_EP_NOTIFY : USB_EP_OUT;
             ep <= (index == 0 ? USB_EP_NOTIFY : USB_EP_IN); ++ep) {
            usb_hw_halt(ep, false); usb_cdc_halt_changed(ep);
        }
        status_in(); return;
    }
    if (configuration && index == USB_CDC_COMM_INTERFACE) {
        if (type == 0x21 && request == 0x20 && value == 0 && length == 7) {
            state = DATA_OUT_LINE; return;
        }
        if (type == 0xa1 && request == 0x21 && value == 0 && length == 7) {
            usb_cdc_get_line_coding(reply); control_read(reply, 7, length); return;
        }
        if (type == 0x21 && request == 0x22 && (value & ~3U) == 0 && length == 0) {
            usb_cdc_control_lines(value); status_in(); return;
        }
    }
    stall(); /* No silent success for unsupported class/vendor/feature requests. */
}

void usb_device_control_out(const uint8_t *data, uint16_t size)
{
    if ((state == STATUS_OUT || state == DATA_IN) && size == 0) {
        /* Host may terminate a control read early with its status OUT. */
        state = IDLE; pending = NONE; usb_hw_control_in_irq(false);
    } else if (state == DATA_OUT_LINE && size == 7 && usb_cdc_valid_line_coding(data)) {
        /* Apply validated class settings before status. A following SETUP
         * can hide the previous status-ACK event; it must not lose settings.
         * Only SET_ADDRESS is explicitly deferred until the status ACK.
         */
        usb_cdc_set_line_coding(data); status_in();
    } else { stall(); }
}

void usb_device_control_in_ready(void)
{
    uint16_t size;
    if (state == DATA_IN) {
        size = remaining > USB_EP_CONTROL_SIZE ? USB_EP_CONTROL_SIZE : remaining;
        usb_hw_control_send(response, size);
        response += size;
        remaining -= size;
        if (size == 0) { send_zlp = false; }
        if (!remaining && !send_zlp) { state = STATUS_OUT; usb_hw_control_in_irq(false); }
    } else if (state == STATUS_IN) {
        if (!status_sent) { usb_hw_control_send(0, 0); status_sent = true; }
        else {
            if (pending == ADDRESS) {
                address = (uint8_t)pending_value;
                usb_hw_set_address(address); /* After host ACK, two separate writes. */
            }
            pending = NONE; state = IDLE; usb_hw_control_in_irq(false);
        }
    } else { usb_hw_control_in_irq(false); }
}

void usb_device_reset(void)
{
    state = IDLE; pending = NONE; address = 0; configuration = 0;
    remaining = 0; suspended = false; status_sent = false; send_zlp = false;
    response = 0;
    usb_cdc_reset();
}

void usb_device_suspend(bool value) { suspended = value; }
bool usb_device_configured(void) { return configuration != 0 && !suspended; }

bool usb_device_begin(void)
{
    /* Read back the emitted identity, including on unconfigured compile-only
     * builds. Keep the whole USB path linked and checked in those builds too.
     */
    const volatile uint8_t *identity = device_descriptor;
    if (active) { usb_device_end(); }
    if ((identity[8] | identity[9]) == 0 || (identity[10] | identity[11]) == 0) { return false; }
    usb_device_reset();
    active = usb_hw_start();
    return active;
}

void usb_device_end(void)
{
    if (active) { usb_hw_stop(); active = false; }
    usb_device_reset();
}
