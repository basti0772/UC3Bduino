# UC3Bduino

Arduino-compatible core for Atmel AVR32 UC3B microcontrollers.

**Stage 1 – Toolchain and bare-metal build infrastructure**

**Arduino API is not implemented yet.**

The long-term goal is an Arduino-compatible UC3B core. This stage builds a small
freestanding C program for **AT32UC3B0256** only. It initializes the stack,
copies `.data`, clears `.bss`, and calls `main()`. No operating system, C library,
Atmel Software Framework (ASF), or board package is linked.

## Requirements

- Windows 11 with Windows PowerShell 5.1 or PowerShell 7.
- A **legacy Atmel AVR32 GNU toolchain**. The tested compiler is
  `avr32-gcc 4.3.2 (atmel-1.2.0-(mingw32_special))`, with binutils
  `2.19.20081016`. Use AVR32 tools, not the similarly named 8-bit AVR tools.
- GNU Make 3.81 or newer (`make.exe` or `mingw32-make.exe`). The tested historical
  toolchain already includes Make 3.81.
- Optional: Python 3.8+ for artifact validation; it is not needed to build.

Obtain the toolchain separately from the
[Microchip archive](https://www.microchip.com/en-us/tools-resources/archives/avr-sam-mcus).
No compiler binaries, vendor headers, or third-party runtime sources are bundled.
See [toolchain details and flags](docs/TOOLCHAIN.md).

## Build on Windows

From the checkout:

```powershell
# Optional when avr32-gcc.exe is already on PATH.
# Replace this example with your installation; root or bin is accepted.
$env:AVR32_TOOLCHAIN_PATH = 'D:\Tools\avr32-toolchain'
.\scripts\build.ps1
```

The script also works when called by its full path from another directory.
An explicit toolchain path takes precedence over PATH; an invalid path fails
with a diagnostic instead of silently selecting another compiler.

If Windows PowerShell blocks local script execution, use this one-process call
after reviewing the script; no persistent execution policy is changed:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

Generated files in `build/at32uc3b0256/minimal/`:

| File | Purpose |
| --- | --- |
| `firmware.elf` | AVR32 executable with debug symbols |
| `firmware.hex` | Intel HEX with flash load addresses |
| `firmware.bin` | Raw flash image starting at `0x80000000` |
| `firmware.map` | Linker memory layout and symbols |

The build prints `avr32-size`. Its BSS total includes the 1 KiB stack reservation;
its text total does not include gaps between aligned flash sections. Use the
map and BIN length when assessing the full flash span.

```powershell
.\scripts\build.ps1 -Example startup-check
python tests/verify_firmware.py build/at32uc3b0256/minimal
python tests/verify_firmware.py build/at32uc3b0256/startup-check --startup-check
.\scripts\clean.ps1 -WhatIf
.\scripts\clean.ps1
```

`startup-check` supplies initialized and zero-initialized objects for debugger
inspection; building it does not execute that check on a controller.
The wrapper rebuilds both objects each time, avoiding stale flags or toolchains.
Build output and locally downloaded tools are excluded from Git.

## Layout

```text
src/main.c                   minimal program
src/startup/uc3_startup.S     original UC startup and exception traps
linker/at32uc3b0256.ld        device memory layout and stack reservation
tools/targets/               device compiler selection
scripts/                    Windows build and clean entry points
examples/                   minimal instructions and startup-check fixture
tests/                      independent ELF/HEX/BIN inspection
docs/                       hardware sources, design, roadmap, validation
```

There is no empty `src/system` or HAL placeholder. Add these when Stage 2 has
actual clock or peripheral code.

## Current limitations

- **Build verified; hardware execution and flashing untested.** See the exact
  [validation record](docs/VALIDATION.md) and [hardware assumptions](docs/HARDWARE.md).
- Full-flash reset image; no bootloader reservation or upload support.
- Reset clock settings are retained. No PLL, watchdog servicing, pin mapping,
  GPIO, INTC dispatch, or timing initialization is supplied.
- Interrupts remain globally masked. Exceptions halt at minimal trap entries.
- C99 only in this stage. C++ constructors, exceptions, heap, libc, and syscalls
  are not supported. Additional UC3B variants require explicit validation.
- No `digitalWrite`, `digitalRead`, `pinMode`, `millis`, `micros`, `Serial`,
  SPI, Wire, ADC, PWM, or Arduino IDE/CLI package.

See [ROADMAP](docs/ROADMAP.md), [ARCHITECTURE](docs/ARCHITECTURE.md), and
[THIRD_PARTY](docs/THIRD_PARTY.md). Original project files use the [MIT license](LICENSE).
