/* SPDX-License-Identifier: MIT */
#include "hal/uc3b_clock.h"
#include "hal/uc3b_gpio.h"

/* Intentionally no default pin: select one from the actual board schematic. */
#if !defined(TEST_GPIO_PORT) || !defined(TEST_GPIO_PIN)
#error "Select a board-verified test pin with build.ps1 -Example gpio-toggle -GpioPort A|B -GpioPin number"
#else
#if !((TEST_GPIO_PORT == 0 && TEST_GPIO_PIN >= 3 && TEST_GPIO_PIN < 32) \
   || (TEST_GPIO_PORT == 1 && TEST_GPIO_PIN >= 0 && TEST_GPIO_PIN < 12))
#error "Invalid smoke-test pin (PA00..PA02 are reserved here to preserve JTAG)"
#endif
#endif

#ifndef TEST_BUSY_WAIT_ITERATIONS
#define TEST_BUSY_WAIT_ITERATIONS 10000UL
#endif
#if TEST_BUSY_WAIT_ITERATIONS < 1 || TEST_BUSY_WAIT_ITERATIONS > 100000000UL
#error "TEST_BUSY_WAIT_ITERATIONS must be between 1 and 100000000"
#endif

/* Debugger-visible observations; building does not execute this example. */
volatile uint32_t gpio_test_cpu_hz;
volatile uint32_t gpio_test_pba_hz;
volatile uint32_t gpio_test_pbb_hz;
volatile bool gpio_test_ready;
volatile bool gpio_test_level;

static void busy_wait(void)
{
    volatile uint32_t remaining = TEST_BUSY_WAIT_ITERATIONS;
    /* Temporary busy wait for hardware smoke test only.
     * Will be replaced by timer-based timing in a later stage.
     * Iterations are not milliseconds and give no calibrated period.
     */
    while (remaining != 0) {
        --remaining;
    }
}

int main(void)
{
    Uc3bGpioPin pin;
    /* Scalar initialization avoids GCC 4.3 emitting a libc memcpy helper. */
    pin.port = TEST_GPIO_PORT;
    pin.pin = TEST_GPIO_PIN;
    uc3b_clock_init();
    gpio_test_cpu_hz = uc3b_clock_get_cpu_hz();
    gpio_test_pba_hz = uc3b_clock_get_pba_hz();
    gpio_test_pbb_hz = uc3b_clock_get_pbb_hz();
    gpio_test_ready = uc3b_gpio_config_output(pin);
    for (;;) {
        if (gpio_test_ready) {
            busy_wait();
            uc3b_gpio_toggle(pin);
            /* Allow pad sampling to settle before the debugger observation. */
            busy_wait();
            gpio_test_level = uc3b_gpio_read(pin);
        }
    }
}
