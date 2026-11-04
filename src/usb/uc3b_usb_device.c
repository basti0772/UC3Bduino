/* SPDX-License-Identifier: MIT
 * AT32UC3B0256 USBB, 32059L ch. 22. Original register/FIFO implementation.
 * No DMA, host mode, GPIO remapping or imported ASF source.
 */
#include "uc3b_usb_device.h"
#include "usb_config.h"
#include "usb_device_core.h"
#include "usb_cdc.h"
#include "usb_fifo.h"
#include "../hal/uc3b_device.h"
#include "../hal/uc3b_irq.h"
#include "../hal/uc3b_clock.h"
#include "../hal/uc3b_system_timer.h"
#include "../hal/uc3b_bootloader.h"

typedef char usb_layout_check[(AVR32_USBB_ADDRESS == 0xfffe0000UL
    && AVR32_USBB_SLAVE_ADDRESS == 0xd0000000UL && AVR32_USBB_IRQ == 544
    && AVR32_USBB_CLK_HSB == 35 && AVR32_USBB_CLK_PBB == 97 && AVR32_PM_GCLK_USBB == 3
    && AVR32_USBB_EPT_NUM == 7
    && __builtin_offsetof(avr32_usbb_t, udcon) == 0
    && __builtin_offsetof(avr32_usbb_t, uerst) == 0x1c
    && __builtin_offsetof(avr32_usbb_t, uecfg0) == 0x100
    && __builtin_offsetof(avr32_usbb_t, uesta0) == 0x130
    && __builtin_offsetof(avr32_usbb_t, uesta0clr) == 0x160
    && __builtin_offsetof(avr32_usbb_t, uecon0) == 0x1c0
    && __builtin_offsetof(avr32_usbb_t, uecon0set) == 0x1f0
    && __builtin_offsetof(avr32_usbb_t, uecon0clr) == 0x220
    && __builtin_offsetof(avr32_usbb_t, usbcon) == 0x800
    && __builtin_offsetof(avr32_usbb_t, usbsta) == 0x804) ? 1 : -1];

#define EP_REG(offset, ep) (*(volatile uint32_t *)(AVR32_USBB_ADDRESS + (offset) + 4UL * (ep)))
#define EP_CFG(ep) EP_REG(AVR32_USBB_UECFG0, ep)
#define EP_STATUS(ep) EP_REG(AVR32_USBB_UESTA0, ep)
#define EP_ACK(ep, bits) (EP_REG(AVR32_USBB_UESTA0CLR, ep) = (bits))
#define EP_ENABLE(ep, bits) (EP_REG(AVR32_USBB_UECON0SET, ep) = (bits))
#define EP_DISABLE(ep, bits) (EP_REG(AVR32_USBB_UECON0CLR, ep) = (bits))
#define EP_CONTROL(ep) EP_REG(AVR32_USBB_UECON0, ep)
#define EP_INTERRUPTS (AVR32_USBB_UECON0_RXSTPE_MASK | AVR32_USBB_UECON0_RXOUTE_MASK | AVR32_USBB_UECON0_TXINE_MASK)
#define BUS_EVENTS (AVR32_USBB_UDINT_EORST_MASK | AVR32_USBB_UDINT_SUSP_MASK | AVR32_USBB_UDINT_SOF_MASK)
static bool running;

uint32_t usb_hw_lock(void) { return uc3b_irq_save(); }
void usb_hw_unlock(uint32_t key) { uc3b_irq_restore(key); }
uint32_t usb_hw_millis(void) { return uc3b_system_timer_millis(); }
bool usb_hw_foreground(void)
{
    uint32_t sr = __builtin_mfsr(AVR32_SR);
    return (sr & (AVR32_SR_GM_MASK | AVR32_SR_I0M_MASK)) == 0
        && ((sr & AVR32_SR_M_MASK) >> AVR32_SR_M_OFFSET) == AVR32_SR_M_SUP;
}

static void barrier(void)
{
    (void)AVR32_USBB.udcon;
    /* CPU=PBB; let the peripheral clear propagate to INTC before rete. */
    __asm__ __volatile__("nop\n\tnop\n\tnop" : : : "memory");
}

static void fifo_read(uint8_t ep, uint8_t *data, uint16_t size)
{
    volatile uint8_t *fifo = (volatile uint8_t *)(AVR32_USBB_SLAVE_ADDRESS + 0x10000UL * ep);
    /* 32059L 22.8.4: each endpoint owns a 64 KiB virtual window. */
    usb_fifo_read_bytes(fifo, data, size);
}
static void fifo_write(uint8_t ep, const uint8_t *data, uint16_t size)
{
    volatile uint8_t *fifo = (volatile uint8_t *)(AVR32_USBB_SLAVE_ADDRESS + 0x10000UL * ep);
    usb_fifo_write_bytes(fifo, data, size);
}
static uint16_t fifo_count(uint8_t ep)
{
    return (uint16_t)((EP_STATUS(ep) & AVR32_USBB_UESTA0_BYCT_MASK) >> AVR32_USBB_UESTA0_BYCT_OFFSET);
}

static void reset_endpoint(uint8_t ep)
{
    uint32_t bit = AVR32_USBB_UERST_EPRST0_MASK << ep;
    AVR32_USBB.uerst |= bit;
    EP_ENABLE(ep, AVR32_USBB_UECON0SET_RSTDTS_MASK);
    AVR32_USBB.uerst &= ~bit;
}
static void free_endpoint(uint8_t ep)
{
    AVR32_USBB.udinteclr = AVR32_USBB_UDINT_EP0INT_MASK << ep;
    AVR32_USBB.uerst &= ~(AVR32_USBB_UERST_EPEN0_MASK << ep);
    EP_CFG(ep) &= ~AVR32_USBB_UECFG0_ALLOC_MASK;
}
static bool allocate_endpoint(uint8_t ep, uint8_t type, bool in, uint8_t size_code)
{
    AVR32_USBB.uerst |= AVR32_USBB_UERST_EPEN0_MASK << ep;
    EP_CFG(ep) = ((uint32_t)type << AVR32_USBB_UECFG0_EPTYPE_OFFSET)
        | (in ? AVR32_USBB_UECFG0_EPDIR_MASK : 0)
        | ((uint32_t)size_code << AVR32_USBB_UECFG0_EPSIZE_OFFSET); /* EPBK=0: one bank */
    EP_CFG(ep) |= AVR32_USBB_UECFG0_ALLOC_MASK;
    if (!(EP_STATUS(ep) & AVR32_USBB_UESTA0_CFGOK_MASK)) { return false; }
    reset_endpoint(ep);
    AVR32_USBB.udinteset = AVR32_USBB_UDINT_EP0INT_MASK << ep;
    return true;
}

bool usb_hw_configure_data(bool enable)
{
    int ep;
    /* DPRAM allocation is ordered; release higher numbers before lower ones. */
    for (ep = USB_EP_IN; ep >= USB_EP_NOTIFY; --ep) { free_endpoint((uint8_t)ep); }
    if (!enable) { return true; }
    if (!allocate_endpoint(USB_EP_NOTIFY, AVR32_USBB_EPTYPE_INTERRUPT, true, AVR32_USBB_EPSIZE_16)
        || !allocate_endpoint(USB_EP_OUT, AVR32_USBB_EPTYPE_BULK, false, AVR32_USBB_EPSIZE_64)
        || !allocate_endpoint(USB_EP_IN, AVR32_USBB_EPTYPE_BULK, true, AVR32_USBB_EPSIZE_64)) {
        for (ep = USB_EP_IN; ep >= USB_EP_NOTIFY; --ep) { free_endpoint((uint8_t)ep); }
        return false;
    }
    usb_hw_out_irq(true);
    return true;
}

static bool bus_reset(void)
{
    int ep;
    usb_device_reset();
    AVR32_USBB.udinteclr = 0xffffffffUL;
    for (ep = AVR32_USBB_EPT_NUM - 1; ep >= 0; --ep) { free_endpoint((uint8_t)ep); }
    usb_hw_set_address(0);
    if (!allocate_endpoint(0, AVR32_USBB_EPTYPE_CONTROL, false, AVR32_USBB_EPSIZE_64)) { return false; }
    EP_ENABLE(0, AVR32_USBB_UECON0_RXSTPE_MASK | AVR32_USBB_UECON0_RXOUTE_MASK);
    AVR32_USBB.udintclr = 0x7f; /* All seven device bus events, not endpoint summaries. */
    AVR32_USBB.udinteset = BUS_EVENTS;
    return true;
}

void usb_hw_set_address(uint8_t address)
{
    uint32_t value = AVR32_USBB.udcon & ~(AVR32_USBB_UDCON_UADD_MASK | AVR32_USBB_UDCON_ADDEN_MASK);
    AVR32_USBB.udcon = value | address;
    if (address) { AVR32_USBB.udcon = value | address | AVR32_USBB_UDCON_ADDEN_MASK; }
}
void usb_hw_control_stall(void) { EP_ENABLE(0, AVR32_USBB_UECON0_STALLRQ_MASK); }
void usb_hw_control_in_irq(bool enable)
{
    if (enable) { EP_ENABLE(0, AVR32_USBB_UECON0_TXINE_MASK); }
    else { EP_DISABLE(0, AVR32_USBB_UECON0_TXINE_MASK); }
}
void usb_hw_control_send(const uint8_t *data, uint16_t size)
{
    fifo_write(0, data, size);
    EP_ACK(0, AVR32_USBB_UESTA0_TXINI_MASK); /* EP0 sends by clearing TXINI, no FIFOCON. */
}
bool usb_hw_halted(uint8_t ep) { return (EP_CONTROL(ep) & AVR32_USBB_UECON0_STALLRQ_MASK) != 0; }
void usb_hw_halt(uint8_t ep, bool halt)
{
    /* Clear both bank ownership and data toggle; discard any in-flight packet.
     * Software RX ring and unsent TX ring remain intact.
     */
    reset_endpoint(ep);
    if (halt) { EP_ENABLE(ep, AVR32_USBB_UECON0_STALLRQ_MASK); }
    else if (ep == USB_EP_OUT) { usb_hw_out_irq(true); }
}
void usb_hw_out_irq(bool enable)
{
    if (enable) { EP_ENABLE(USB_EP_OUT, AVR32_USBB_UECON0_RXOUTE_MASK); }
    else { EP_DISABLE(USB_EP_OUT, AVR32_USBB_UECON0_RXOUTE_MASK); }
}
bool usb_hw_send(uint8_t ep, const uint8_t *data, uint16_t size)
{
    if (!(EP_STATUS(ep) & AVR32_USBB_UESTA0_TXINI_MASK)) { return false; }
    /* Non-control IN: clear ready, fill bank, then release FIFOCON. */
    EP_ACK(ep, AVR32_USBB_UESTA0_TXINI_MASK);
    fifo_write(ep, data, size);
    EP_DISABLE(ep, AVR32_USBB_UECON0_FIFOCON_MASK);
    EP_ENABLE(ep, AVR32_USBB_UECON0_TXINE_MASK);
    return true;
}

bool usb_hw_start(void)
{
    uint32_t wait, start;
    extern const char __usb_vector_offset[];
    if (!uc3b_clock_enable_usb()) { return false; }
    AVR32_PM.hsbmask |= 1UL << (AVR32_USBB_CLK_HSB % 32);
    AVR32_PM.pbbmask |= 1UL << (AVR32_USBB_CLK_PBB % 32);
    AVR32_PM.pbamask |= 1UL << (AVR32_INTC_CLK_PBA % 32);
    for (wait = 0; wait < 100000UL; ++wait) {
        if (AVR32_PM.poscsr & AVR32_PM_POSCSR_MSKRDY_MASK) { break; }
    }
    if (wait == 100000UL) { uc3b_clock_disable_usb(); return false; }
    AVR32_USBB.udcon = AVR32_USBB_UDCON_DETACH_MASK;
    AVR32_USBB.usbcon = AVR32_USBB_USBCON_UIMOD_MASK; /* USBE=0 resets controller. */
    /* Deliberate soft disconnect across two full-speed frame periods (2 ms):
     * covers a host-driven packet/SOF and USB 2.0 7.1.7.3 / table 7-13 TDDIS
     * (2.5 us max). COUNT uses the unchanged CPU clock, interrupts stay enabled.
     */
    start = __builtin_mfsr(AVR32_COUNT);
    while ((uint32_t)(__builtin_mfsr(AVR32_COUNT) - start) < uc3b_clock_get_cpu_hz() / 500UL) { }
    AVR32_USBB.usbcon = AVR32_USBB_USBCON_UIMOD_MASK | AVR32_USBB_USBCON_OTGPADE_MASK
        | AVR32_USBB_USBCON_USBE_MASK; /* UIDE=0, FRZCLK=0, device; no ID/VBOF mux. */
    for (wait = 0; wait < 100000UL; ++wait) {
        if (AVR32_USBB.usbsta & AVR32_USBB_USBSTA_CLKUSABLE_MASK) { break; }
    }
    if (wait == 100000UL || !bus_reset()) {
        AVR32_USBB.usbcon = AVR32_USBB_USBCON_UIMOD_MASK;
        uc3b_clock_disable_usb(); return false;
    }
    AVR32_INTC.ipr[AVR32_USBB_IRQ / 32] = (uint32_t)__usb_vector_offset
        | (AVR32_INTC_INTLEVEL_INT2 << AVR32_INTC_INTLEVEL_OFFSET);
    AVR32_USBB.usbstaclr = AVR32_USBB_USBSTA_VBUSTI_MASK;
    running = true;
    AVR32_USBB.usbcon |= AVR32_USBB_USBCON_VBUSTE_MASK;
    /* Hardware only pulls D+ high when VBUS is present (22.7.1.8). */
    AVR32_USBB.udcon &= ~AVR32_USBB_UDCON_DETACH_MASK;
    barrier();
    __asm__ __volatile__("csrf %0" : : "i" (AVR32_SR_I2M) : "memory");
    return true;
}

void usb_hw_stop(void)
{
    uint32_t key = usb_hw_lock();
    running = false;
    AVR32_USBB.udinteclr = 0xffffffffUL;
    AVR32_USBB.udcon = AVR32_USBB_UDCON_DETACH_MASK;
    AVR32_USBB.usbcon = AVR32_USBB_USBCON_UIMOD_MASK;
    barrier();
    uc3b_clock_disable_usb();
    usb_hw_unlock(key);
}

void usb_hw_request_bootloader(void)
{
    /* Called only after the CDC close request's status ACK and a grace period.
     * Failed preparation leaves the running application and USB intact.
     */
    if (!uc3b_bootloader_prepare()) { return; }
    usb_hw_stop();
    uc3b_system_reset();
}

__attribute__((__interrupt__)) void uc3b_usb_isr(void)
{
    uint32_t events, status;
    uint16_t size;
    uint8_t packet[USB_EP_CONTROL_SIZE], ep;
    if (!running) { barrier(); return; }
    if (AVR32_USBB.usbsta & AVR32_USBB_USBSTA_VBUSTI_MASK) {
        AVR32_USBB.usbstaclr = AVR32_USBB_USBSTA_VBUSTI_MASK;
        /* Clearing both HW/software state on either edge handles reconnects
         * without trusting the previous host's configuration or data toggles.
         */
        AVR32_USBB.udcon |= AVR32_USBB_UDCON_DETACH_MASK;
        if (bus_reset()) { AVR32_USBB.udcon &= ~AVR32_USBB_UDCON_DETACH_MASK; }
        barrier(); return;
    }
    events = AVR32_USBB.udint & AVR32_USBB.udinte;
    if (events & AVR32_USBB_UDINT_EORST_MASK) {
        if (!bus_reset()) { AVR32_USBB.udcon |= AVR32_USBB_UDCON_DETACH_MASK; }
        barrier(); return;
    }
    if (events & AVR32_USBB_UDINT_SUSP_MASK) {
        usb_device_suspend(true);
        AVR32_USBB.udintclr = AVR32_USBB_UDINT_SUSP_MASK;
        AVR32_USBB.udinteclr = AVR32_USBB_UDINT_SUSP_MASK;
        AVR32_USBB.udinteset = AVR32_USBB_UDINT_WAKEUP_MASK;
    }
    if (events & AVR32_USBB_UDINT_WAKEUP_MASK) {
        usb_device_suspend(false);
        AVR32_USBB.udintclr = AVR32_USBB_UDINT_WAKEUP_MASK | AVR32_USBB_UDINT_EORSM_MASK;
        AVR32_USBB.udinteclr = AVR32_USBB_UDINT_WAKEUP_MASK;
        AVR32_USBB.udinteset = AVR32_USBB_UDINT_SUSP_MASK;
    }
    if (events & AVR32_USBB_UDINT_EP0INT_MASK) {
        status = EP_STATUS(0) & EP_CONTROL(0) & EP_INTERRUPTS;
        /* New SETUP wins over stale IN/OUT. Status OUT wins over simultaneous
         * TXINI at the end of a control read (32059L 22.7.2.9).
         */
        if (status & AVR32_USBB_UESTA0_RXSTPI_MASK) {
            size = fifo_count(0);
            if (size == 8) { fifo_read(0, packet, 8); }
            /* Discard an old OUT/status indication that raced the new SETUP.
             * Do not clear TXINI here: for EP0 that would transmit a packet.
             */
            EP_ACK(0, AVR32_USBB_UESTA0_RXOUTI_MASK | AVR32_USBB_UESTA0_RXSTPI_MASK);
            if (size == 8) { usb_device_setup(packet); }
            else { usb_hw_control_in_irq(false); usb_hw_control_stall(); }
        } else if (status & AVR32_USBB_UESTA0_RXOUTI_MASK) {
            size = fifo_count(0);
            if (size <= sizeof(packet)) { fifo_read(0, packet, size); }
            EP_ACK(0, AVR32_USBB_UESTA0_RXOUTI_MASK);
            if (size <= sizeof(packet)) { usb_device_control_out(packet, size); }
            else { usb_hw_control_in_irq(false); usb_hw_control_stall(); }
        } else if (status & AVR32_USBB_UESTA0_TXINI_MASK) { usb_device_control_in_ready(); }
    }
    if (events & (AVR32_USBB_UDINT_EP0INT_MASK << USB_EP_OUT)) {
        size = fifo_count(USB_EP_OUT);
        if (usb_cdc_can_receive(size)) {
            EP_ACK(USB_EP_OUT, AVR32_USBB_UESTA0_RXOUTI_MASK);
            fifo_read(USB_EP_OUT, packet, size);
            usb_cdc_receive(packet, size);
            EP_DISABLE(USB_EP_OUT, AVR32_USBB_UECON0_FIFOCON_MASK);
        } else { usb_hw_out_irq(false); } /* Keep bank; hardware NAKs without data loss. */
    }
    for (ep = USB_EP_NOTIFY; ep <= USB_EP_IN; ep += 2) {
        if (events & (AVR32_USBB_UDINT_EP0INT_MASK << ep)) {
            EP_DISABLE(ep, AVR32_USBB_UECON0_TXINE_MASK);
            usb_cdc_in_complete(ep); /* Leave TXINI set until next queued IN. */
        }
    }
    if (events & AVR32_USBB_UDINT_SOF_MASK) {
        AVR32_USBB.udintclr = AVR32_USBB_UDINT_SOF_MASK;
        usb_cdc_sof();
    }
    barrier();
}
