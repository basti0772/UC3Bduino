/* SPDX-License-Identifier: MIT */
#include <Arduino.h>
#include "hal/uc3b_clock.h"

void init(void)
{
    uc3b_clock_init();
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
