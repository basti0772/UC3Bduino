/* SPDX-License-Identifier: MIT */
#include "usb_cdc.h"
#include "usb_device_core.h"
#include "uc3b_usb_device.h"
#include "usb_config.h"

static uint8_t rx[USB_SERIAL_RX_BUFFER_SIZE], tx[USB_SERIAL_TX_BUFFER_SIZE];
static uint16_t rx_head, rx_tail, tx_head, tx_tail, control_lines;
static uint8_t line_coding[7];
static bool tx_busy, tx_zlp, notify_busy, notify_dirty;
#define RX_MASK (USB_SERIAL_RX_BUFFER_SIZE - 1)
#define TX_MASK (USB_SERIAL_TX_BUFFER_SIZE - 1)

static bool ready(void) { return usb_device_configured() && (control_lines & 1) != 0; }

void usb_cdc_reset(void)
{
    rx_head = rx_tail = tx_head = tx_tail = control_lines = 0;
    tx_busy = tx_zlp = notify_busy = false;
    notify_dirty = true;
    line_coding[0] = 0; line_coding[1] = 0xc2; line_coding[2] = 1; line_coding[3] = 0;
    line_coding[4] = 0; line_coding[5] = 0; line_coding[6] = 8; /* 115200 8N1 */
}

bool usb_cdc_begin(void) { return usb_hw_foreground() && usb_device_begin(); }
void usb_cdc_end(void) { if (usb_hw_foreground()) { usb_device_end(); } }

void usb_cdc_control_lines(uint16_t value)
{
    if ((control_lines ^ value) & 1) { notify_dirty = true; }
    control_lines = value; /* DTR and RTS retained; native CDC has no UART pins. */
}

bool usb_cdc_valid_line_coding(const uint8_t data[7])
{
    return data[4] <= 2 && data[5] <= 4
        && ((data[6] >= 5 && data[6] <= 8) || data[6] == 16);
}

void usb_cdc_get_line_coding(uint8_t data[7])
{
    uint8_t i;
    for (i = 0; i < 7; ++i) { data[i] = line_coding[i]; }
}
void usb_cdc_set_line_coding(const uint8_t data[7])
{
    uint8_t i;
    for (i = 0; i < 7; ++i) { line_coding[i] = data[i]; }
}

bool usb_cdc_can_receive(uint16_t size)
{
    return size <= USB_EP_DATA_SIZE && size <= ((rx_tail - rx_head - 1) & RX_MASK);
}
void usb_cdc_receive(const uint8_t *data, uint16_t size)
{
    while (size--) { rx[rx_head] = *data++; rx_head = (rx_head + 1) & RX_MASK; }
}

static void transmit(void)
{
    uint8_t packet[USB_EP_DATA_SIZE];
    uint16_t size = 0, next = tx_tail;
    if (!ready() || tx_busy || usb_hw_halted(USB_EP_IN)) { return; }
    while (next != tx_head && size < USB_EP_DATA_SIZE) {
        packet[size++] = tx[next]; next = (next + 1) & TX_MASK;
    }
    if ((size || tx_zlp) && usb_hw_send(USB_EP_IN, packet, size)) {
        tx_tail = next;
        tx_busy = true;
        tx_zlp = size == USB_EP_DATA_SIZE;
    }
}

void usb_cdc_sof(void)
{
    uint8_t notification[10] = { 0xa1, 0x20, 0, 0, USB_CDC_COMM_INTERFACE, 0, 2, 0, 0, 0 };
    if (!usb_device_configured()) { return; }
    /* Native-port policy: report DSR/DCD while host asserts DTR; no fictitious
     * UART error/break indications. Only send on a state change or config/reset.
     */
    if (notify_dirty && !notify_busy && !usb_hw_halted(USB_EP_NOTIFY)) {
        notification[8] = (control_lines & 1) ? 3 : 0;
        if (usb_hw_send(USB_EP_NOTIFY, notification, sizeof(notification))) {
            notify_busy = true; notify_dirty = false;
        }
    }
    transmit(); /* Batch Print's byte writes, at most one packet per 1 ms SOF. */
}

void usb_cdc_in_complete(uint8_t ep)
{
    if (ep == USB_EP_NOTIFY) { notify_busy = false; }
    else if (ep == USB_EP_IN) { tx_busy = false; }
    /* Next SOF sends more data or the terminating ZLP after a full final packet.
     * No self-retriggering TX-ready interrupt while the software queue is empty.
     */
}
void usb_cdc_halt_changed(uint8_t ep)
{
    if (ep == USB_EP_IN) { tx_busy = false; tx_zlp = false; }
    if (ep == USB_EP_NOTIFY) { notify_busy = false; notify_dirty = true; }
}

bool usb_cdc_ready(void)
{
    uint32_t key = usb_hw_lock();
    bool result = ready();
    usb_hw_unlock(key); return result;
}
int usb_cdc_available(void)
{
    uint32_t key = usb_hw_lock();
    int count = (rx_head - rx_tail) & RX_MASK;
    usb_hw_unlock(key); return count;
}
int usb_cdc_peek(void)
{
    uint32_t key = usb_hw_lock();
    int value = rx_head == rx_tail ? -1 : rx[rx_tail];
    usb_hw_unlock(key); return value;
}
int usb_cdc_read(void)
{
    uint32_t key = usb_hw_lock();
    int value = -1;
    if (rx_head != rx_tail) {
        value = rx[rx_tail]; rx_tail = (rx_tail + 1) & RX_MASK;
        /* Re-enable only once a complete maximum-size OUT fits. A held FIFO
         * bank then retriggers RXOUTI; no need to wait for a new host packet.
         */
        if (usb_device_configured() && usb_cdc_can_receive(USB_EP_DATA_SIZE)) { usb_hw_out_irq(true); }
    }
    usb_hw_unlock(key); return value;
}
bool usb_cdc_write(uint8_t value)
{
    uint32_t key = usb_hw_lock();
    uint16_t next = (tx_head + 1) & TX_MASK;
    bool accepted = ready() && next != tx_tail;
    if (accepted) { tx[tx_head] = value; tx_head = next; }
    usb_hw_unlock(key); return accepted;
}
void usb_cdc_flush(void)
{
    uint32_t start, key;
    bool done;
    /* Foreground only, with interrupts enabled. Never wait in a USB ISR or a
     * critical section. Timer is the existing Arduino TC timebase, not USB SOF.
     */
    if (!usb_hw_foreground()) { return; }
    start = usb_hw_millis();
    do {
        key = usb_hw_lock();
        transmit();
        done = !ready() || (tx_head == tx_tail && !tx_busy && !tx_zlp)
            || usb_hw_halted(USB_EP_IN);
        usb_hw_unlock(key);
        if (done) { return; }
    } while ((uint32_t)(usb_hw_millis() - start) < USB_CDC_FLUSH_TIMEOUT_MS);
    /* On timeout, pending TX remains queued/in flight. RX is never cleared. */
}
