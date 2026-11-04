/* SPDX-License-Identifier: MIT */
#include "uc3b_bootloader.h"
#include "uc3b_bootloader_hw.h"

#if UC3BDUINO_FACTORY_DFU_GP31
static bool boot_wait_ready(void)
{
    uint32_t tries;
    for (tries = 0; tries < 100000UL; ++tries) {
        if (boot_hw_ready()) { return true; }
    }
    return false;
}
#endif

bool uc3b_bootloader_prepare(void)
{
#if UC3BDUINO_FACTORY_DFU_GP31
    uint32_t key = boot_hw_lock(), previous;
    bool ok = false;
    if (!boot_hw_supported()) { goto done; }
    if (!boot_wait_ready()) { goto done; }
    previous = boot_hw_fuses();
    if (((previous >> 17) & 7UL) != 3UL) { goto done; }
    if (!(previous & 0x80000000UL)) {
        /* AVR32784 7.2: ISP_FORCE is GP31. No page, user-page or other fuse
         * command is issued. This is NOT a volatile SRAM boot marker.
         */
        boot_hw_set_force();
        if (!boot_wait_ready() || !boot_hw_command_ok()) { goto done; }
    }
    ok = boot_hw_fuses() == (previous | 0x80000000UL);
done:
    boot_hw_unlock(key);
    return ok;
#else
    return false;
#endif
}

void uc3b_system_reset(void)
{
    boot_hw_reset();
}
