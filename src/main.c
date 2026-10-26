/* SPDX-License-Identifier: MIT */
#include "hal/uc3b_clock.h"

int main(void)
{
    uc3b_clock_init();
    for (;;) {
        /* Known nominal clock; no GPIO pin is selected by this example. */
    }
}
