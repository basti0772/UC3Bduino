/* SPDX-License-Identifier: MIT */
#include <Arduino.h>
#include "hal/uc3b_clock.h"
#include "hal/uc3b_system_timer.h"

void init(void)
{
    uc3b_clock_init();
    if (!uc3b_system_timer_init()) {
        for (;;) { /* Unsupported clock/init context: do not run invalid time. */
        }
    }
}

extern "C" int main(void)
{
    init();
    setup();
    for (;;) {
        loop();
    }
    return 0; /* Required by the historical freestanding C++ compiler. */
}
