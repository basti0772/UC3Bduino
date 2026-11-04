/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define UC3B_BOOTLOADER_HW_H
static uint32_t fuses, unexpected, locks;
static unsigned writes, polls, resets;
static bool ready_before, ready_after, command_ok, supported;
static bool boot_hw_supported(void) { return supported; }
static uint32_t boot_hw_lock(void) { assert(!locks); return locks++; }
static void boot_hw_unlock(uint32_t key) { assert(locks == key + 1); --locks; }
static bool boot_hw_ready(void) { ++polls; return writes ? ready_after : ready_before; }
static uint32_t boot_hw_fuses(void) { return fuses; }
static bool boot_hw_command_ok(void) { return command_ok; }
static void boot_hw_set_force(void) { ++writes; fuses |= 0x80000000UL; fuses ^= unexpected; }
static void boot_hw_reset(void) { ++resets; }
#include "../src/hal/uc3b_bootloader.c"
static void fresh(void)
{
    fuses = 3UL << 17; writes = polls = unexpected = 0;
    ready_before = ready_after = command_ok = supported = true;
}
int main(void)
{
    fresh();
#if UC3BDUINO_FACTORY_DFU_GP31
    assert(uc3b_bootloader_prepare()); assert(writes == 1 && fuses == 0x80060000UL && !locks);
    assert(uc3b_bootloader_prepare()); assert(writes == 1 && !locks); /* already set */
    fresh(); supported = false; assert(!uc3b_bootloader_prepare()); assert(!writes && !polls && !locks);
    fresh(); fuses = 2UL << 17; assert(!uc3b_bootloader_prepare()); assert(!writes && !locks);
    fresh(); ready_before = false; assert(!uc3b_bootloader_prepare()); assert(!writes && polls == 100000 && !locks);
    fresh(); ready_after = false; assert(!uc3b_bootloader_prepare()); assert(writes == 1 && polls == 100001 && !locks);
    fresh(); command_ok = false; assert(!uc3b_bootloader_prepare()); assert(writes == 1 && !locks);
    fresh(); unexpected = 1; assert(!uc3b_bootloader_prepare()); assert(writes == 1 && !locks);
#else
    assert(!uc3b_bootloader_prepare()); assert(!writes && !polls && !locks);
#endif
    uc3b_system_reset(); assert(resets == 1);
    puts("PASS: legacy boot backend opt-in, BOOTPROT, bounded readiness, exact GP31 change, command error and IRQ restoration");
    return 0;
}
