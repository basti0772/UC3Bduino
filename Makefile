# SPDX-License-Identifier: MIT
# GNU Make 3.81 compatible. The Windows wrapper creates the output directory.
.DEFAULT_GOAL := all
.SUFFIXES:
.DELETE_ON_ERROR:

include tools/targets/at32uc3b0256.mk

# build.ps1 supplies a quoted absolute prefix, including paths with spaces.
# Direct make users can put the toolchain's bin directory on PATH.
TOOLCHAIN_BIN ?=
TOOL_PREFIX := $(if $(strip $(TOOLCHAIN_BIN)),$(TOOLCHAIN_BIN)/,)avr32-
AVR32_CC := $(TOOL_PREFIX)gcc
AVR32_OBJCOPY := $(TOOL_PREFIX)objcopy
AVR32_SIZE := $(TOOL_PREFIX)size

# Relative paths keep GCC 4.3 debug paths and make dependency files portable.
BUILD_DIR ?= build/at32uc3b0256
APP_SOURCE ?= src/main.c
OBJECTS := $(BUILD_DIR)/startup.o $(BUILD_DIR)/main.o
TARGET_FLAGS := -mpart=$(AVR32_PART) -mno-relax
CFLAGS := $(TARGET_FLAGS) -std=c99 -Os -g -ffreestanding -fno-builtin \
          -fno-common -ffunction-sections -fdata-sections -muse-rodata-section \
          -Wall -Wextra -Werror
ASFLAGS := $(TARGET_FLAGS) -g
LDFLAGS := $(TARGET_FLAGS) -nostdlib -nostartfiles \
           -Wl,--gc-sections -Wl,--no-undefined -Wl,-Map,$(BUILD_DIR)/firmware.map \
           -T$(LINKER_SCRIPT)

.PHONY: all size
all: $(BUILD_DIR)/firmware.elf $(BUILD_DIR)/firmware.hex $(BUILD_DIR)/firmware.bin size

$(BUILD_DIR)/startup.o: src/startup/uc3_startup.S Makefile tools/targets/at32uc3b0256.mk
	"$(AVR32_CC)" $(ASFLAGS) -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/main.o: $(APP_SOURCE) Makefile tools/targets/at32uc3b0256.mk
	"$(AVR32_CC)" $(CFLAGS) -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/firmware.elf: $(OBJECTS) $(LINKER_SCRIPT)
	"$(AVR32_CC)" $(LDFLAGS) $(OBJECTS) -o "$@"

$(BUILD_DIR)/firmware.hex: $(BUILD_DIR)/firmware.elf
	"$(AVR32_OBJCOPY)" -O ihex "$<" "$@"

$(BUILD_DIR)/firmware.bin: $(BUILD_DIR)/firmware.elf
	"$(AVR32_OBJCOPY)" -O binary "$<" "$@"

size: $(BUILD_DIR)/firmware.elf
	"$(AVR32_SIZE)" "$<"

-include $(OBJECTS:.o=.d)
