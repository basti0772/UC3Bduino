/* SPDX-License-Identifier: MIT */
#ifndef UC3B_BOOTLOADER_H
#define UC3B_BOOTLOADER_H
#include <stdbool.h>
#ifndef UC3BDUINO_FACTORY_DFU_GP31
#define UC3BDUINO_FACTORY_DFU_GP31 0
#endif
#if UC3BDUINO_FACTORY_DFU_GP31 != 0 && UC3BDUINO_FACTORY_DFU_GP31 != 1
#error "UC3BDUINO_FACTORY_DFU_GP31 must be 0 or 1"
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Off by default. The opt-in legacy backend changes nonvolatile GP fuse 31. */
bool uc3b_bootloader_prepare(void);
void uc3b_system_reset(void);
#ifdef __cplusplus
}
#endif
#endif
