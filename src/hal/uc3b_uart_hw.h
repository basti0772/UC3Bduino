/* SPDX-License-Identifier: MIT */
#ifndef UC3B_UART_HW_H
#define UC3B_UART_HW_H
#include "uc3b_device.h"
#include "uc3b_gpio.h"
#include "uc3b_irq.h"

#define UC3B_UART_ISR __attribute__((__interrupt__))
#define UC3B_UART_RX_READY AVR32_USART_CSR_RXRDY_MASK
#define UC3B_UART_TX_READY AVR32_USART_CSR_TXRDY_MASK
#define UC3B_UART_TX_EMPTY AVR32_USART_CSR_TXEMPTY_MASK
#define UC3B_UART_ERRORS (AVR32_USART_CSR_OVRE_MASK | AVR32_USART_CSR_FRAME_MASK | AVR32_USART_CSR_PARE_MASK)
#define uart_hw_irq_save uc3b_irq_save
#define uart_hw_irq_restore uc3b_irq_restore

/* 32059L: memory/interrupt/clock maps, table 4-1, chapter 21. */
typedef char uc3b_uart_layout_check[
    (AVR32_USART0_ADDRESS == 0xFFFF1400UL && AVR32_USART0_IRQ == 160 && AVR32_USART0_CLK_PBA == 71
     && AVR32_USART1_ADDRESS == 0xFFFF1800UL && AVR32_USART1_IRQ == 192 && AVR32_USART1_CLK_PBA == 72
     && AVR32_USART2_ADDRESS == 0xFFFF1C00UL && AVR32_USART2_IRQ == 224 && AVR32_USART2_CLK_PBA == 73
     && __builtin_offsetof(avr32_usart_t, cr) == 0x00
     && __builtin_offsetof(avr32_usart_t, mr) == 0x04
     && __builtin_offsetof(avr32_usart_t, ier) == 0x08
     && __builtin_offsetof(avr32_usart_t, idr) == 0x0c
     && __builtin_offsetof(avr32_usart_t, csr) == 0x14
     && __builtin_offsetof(avr32_usart_t, rhr) == 0x18
     && __builtin_offsetof(avr32_usart_t, thr) == 0x1c
     && __builtin_offsetof(avr32_usart_t, brgr) == 0x20) ? 1 : -1];

typedef struct {
    volatile avr32_usart_t *regs;
    uint8_t clock_bit, irq_group, rx, tx, function;
} UartRoute;
#define ROUTE(n, r) { &AVR32_USART##n, AVR32_USART##n##_CLK_PBA % 32, AVR32_USART##n##_IRQ / 32, \
    AVR32_USART##n##_RXD_0_##r##_PIN, AVR32_USART##n##_TXD_0_##r##_PIN, AVR32_USART##n##_TXD_0_##r##_FUNCTION }
static const UartRoute routes[] = {
    ROUTE(0, 0), ROUTE(0, 1), ROUTE(1, 0), ROUTE(1, 1), ROUTE(2, 0), ROUTE(2, 1)
};
#undef ROUTE
static const UartRoute *uart_route;

static inline Uc3bGpioPin uart_hw_pin(uint8_t number)
{
    Uc3bGpioPin pin;
    pin.port = number / 32;
    pin.pin = number % 32;
    return pin;
}

static inline bool uart_hw_route_valid(uint8_t route)
{
#if UC3B_CLOCK_SOURCE == UC3B_CLOCK_SOURCE_OSC0
    if (route == UC3B_UART_U0A) { return false; } /* RX conflicts with XIN0. */
#endif
    return route < sizeof(routes) / sizeof(routes[0]);
}

static inline uint32_t uart_hw_status(void) { return uart_route->regs->csr; }
static inline uint8_t uart_hw_read(void) { return (uint8_t)uart_route->regs->rhr; }
static inline void uart_hw_write(uint8_t value) { uart_route->regs->thr = value; }
static inline void uart_hw_clear_errors(void) { uart_route->regs->cr = AVR32_USART_CR_RSTSTA_MASK; }

static inline void uart_hw_ack_barrier(void)
{
    /* CSR does not consume a new byte. Allow IRQ deassertion to reach INTC
     * (three CLK_SYNC cycles, CPU=PBA here); never dummy-read RHR.
     */
    (void)uart_route->regs->csr;
    __asm__ __volatile__("nop\n\tnop\n\tnop" : : : "memory");
}

static inline void uart_hw_configure(uint8_t route, uint32_t cd)
{
    extern const char __uart_vector_offset[];
    volatile avr32_usart_t *regs;
    uart_route = &routes[route];
    regs = uart_route->regs;
    AVR32_PM.pbamask |= (1UL << uart_route->clock_bit) | (1UL << (AVR32_INTC_CLK_PBA % 32));
    while ((AVR32_PM.poscsr & AVR32_PM_POSCSR_MSKRDY_MASK) == 0) { }
    regs->idr = 0xffffffffUL; /* Disable all sources; reserved bits have no effect. */
    regs->cr = AVR32_USART_CR_RSTRX_MASK | AVR32_USART_CR_RSTTX_MASK | AVR32_USART_CR_RSTSTA_MASK;
    regs->mr = (AVR32_USART_MR_MODE_NORMAL << AVR32_USART_MR_MODE_OFFSET)
        | (AVR32_USART_MR_USCLKS_MCK << AVR32_USART_MR_USCLKS_OFFSET)
        | (AVR32_USART_MR_CHRL_8 << AVR32_USART_MR_CHRL_OFFSET)
        | (AVR32_USART_MR_PAR_NONE << AVR32_USART_MR_PAR_OFFSET)
        | (AVR32_USART_MR_NBSTOP_1 << AVR32_USART_MR_NBSTOP_OFFSET)
        | AVR32_USART_MR_OVER_MASK;
    regs->brgr = cd; /* FP=0, PBA / (8*CD). */
    (void)uc3b_gpio_select_peripheral(uart_hw_pin(uart_route->rx), uart_route->function);
    (void)uc3b_gpio_select_peripheral(uart_hw_pin(uart_route->tx), uart_route->function);
    AVR32_INTC.ipr[uart_route->irq_group] = (uint32_t)__uart_vector_offset
        | (AVR32_INTC_INTLEVEL_INT1 << AVR32_INTC_INTLEVEL_OFFSET);
    regs->cr = AVR32_USART_CR_RXEN_MASK | AVR32_USART_CR_TXEN_MASK;
    regs->ier = UC3B_UART_RX_READY | UC3B_UART_ERRORS;
    uart_hw_ack_barrier();
}

static inline void uart_hw_enable(void)
{
    __asm__ __volatile__("csrf %0" : : "i" (AVR32_SR_I1M) : "memory");
}

static inline void uart_hw_disable(void)
{
    uart_route->regs->idr = 0xffffffffUL;
    uart_route->regs->cr = AVR32_USART_CR_RSTRX_MASK | AVR32_USART_CR_RSTTX_MASK | AVR32_USART_CR_RSTSTA_MASK;
    uart_hw_ack_barrier();
    /* Release just these two pads as floating inputs; clocks remain enabled. */
    (void)uc3b_gpio_config_input(uart_hw_pin(uart_route->rx));
    (void)uc3b_gpio_config_input(uart_hw_pin(uart_route->tx));
}
#endif
