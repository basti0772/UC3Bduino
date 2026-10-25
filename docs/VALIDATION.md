# Validation record

For the current **Stage 2.5 – Arduino IDE compilation smoke test**, see
[TESTING](TESTING.md): actual CLI builds, installer checks, regression builds
and the explicit distinction between CLI, IDE GUI and hardware results.
The records below describe earlier stages.

## Stage 2 — executed on 2026-10-25

**Build tested: yes. Hardware tested: no.** No firmware was flashed and no
physical board or AVR32 simulator was used. Passing builds do not prove the
PM handshake, actual clock frequency, pad output, input or pull-up behavior.

The installed AVR32-GCC 4.3.2, binutils 2.19.20081016 and Make 3.81 were used on
Windows 11. Build commands ran through Windows PowerShell 5.1, orchestrated by
PowerShell 7.6.5. The same legacy-tool sandbox limitation described below
required authorized host execution. No toolchain was downloaded or replaced.

All three examples compiled and linked with `-Wall -Wextra -Werror`,
`-nostdlib -nostartfiles`, with successful ELF/HEX/BIN/map generation:

| Example | `avr32-size` text | data | bss (includes stack) | BIN bytes |
| --- | ---: | ---: | ---: | ---: |
| minimal | 680 | 0 | 1024 | 1104 |
| startup-check | 740 | 8 | 1036 | 1172 |
| gpio-toggle (PA03, compilation only) | 1088 | 0 | 1044 | 1512 |

The original artifact verifier passed on every final example. It checks ELF
machine/entry, flash load bytes, SRAM/stack bounds, undefined globals, HEX
checksums and ELF/HEX/BIN agreement. The startup fixture still checks its data
initializers in the artifact; it has not been executed on a controller.

Additional checks performed:

- GPIO builds at PA03, PA31, PB00 and PB11; PB00 also used one busy-wait iteration.
  These are compile-time cases, not board pin assignments or hardware results.
- Repeated PA03 builds produced identical ELF/HEX/BIN/map SHA-256 hashes.
- Missing/partial pin options, PB12, PA02 (reserved for JTAG in the example),
  port C, pin 32, zero busy-wait count and GPIO options on `minimal` all fail
  with nonzero status. Direct compiler probes reject missing/invalid pins,
  an unsupported clock source and `-mpart=uc3b1256`.
- Every HAL API was force-retained in an extra scratch ELF using linker `-u`
  options. It links with no undefined symbols and no libc/libgcc/CRT. This
  covers APIs otherwise removed from the toggle example by section GC.
- Disassembly shows full-width GPIO alias stores at documented offsets and
  PVR reads. Validation branches precede pointer indexing and shifts; no
  read-modify-write of GPIO output registers occurs. PM uses ordinary loads
  and stores, not peripheral memory RMW instructions.
- Reset disassembly contains `ssrf GM`, two NOPs, then SP/EVBA setup. This small
  correction follows the Rev B erratum; the linker and remaining startup are
  unchanged. No peripheral interrupts are enabled.
- Compile-time checks verify used vendor register offsets, 32-bit register
  words, two banks and 0x100-byte bank stride. Compiler dependency output was
  compared with the [header manifest](DEVICE_HEADERS.md).

The first compile exposed the historical headers' anonymous unions: the HAL
requires GCC's documented `-fms-extensions` option in C99 mode. Warnings remain
errors. A constant local aggregate initializer in the example also caused GCC
4.3 to request `memcpy`; scalar field initialization avoids that dependency.
Neither issue was solved by adding a runtime library or suppressing warnings.

### Reproduce Stage 2

```powershell
.\scripts\build.ps1 -Example minimal
.\scripts\build.ps1 -Example startup-check
# Compilation example only; verify your board before flashing any chosen pin.
.\scripts\build.ps1 -Example gpio-toggle -GpioPort A -GpioPin 3
python tests/verify_firmware.py build/at32uc3b0256/minimal
python tests/verify_firmware.py build/at32uc3b0256/startup-check --startup-check
python tests/verify_firmware.py build/at32uc3b0256/gpio-toggle
avr32-objdump -d build/at32uc3b0256/gpio-toggle/uc3b_clock.o
avr32-objdump -d build/at32uc3b0256/gpio-toggle/uc3b_gpio.o
avr32-nm -u build/at32uc3b0256/gpio-toggle/firmware.elf
```

Toolchain inspection commands need its bin directory on PATH. Boundary/error
fixtures and logs remain in ignored scratch storage; no mocking/test framework
was added. Runtime pin validation was inspected, not executed on a host or MCU.

### Remaining local hardware tests

Follow the [toggle procedure](../examples/gpio_toggle/README.md) and
[GPIO checks](GPIO.md): identify board/silicon/fuses, verify reset-to-main and PM
registers, choose a safe physical pad, measure toggles, check set/clear/input and
pull-ups, read PVR, and confirm unrelated pads remain unchanged. Record actual
RC timing; nominal 115200 Hz is not a measurement. Debugger/trace ownership,
oscillator wiring and bootloader coexistence remain TODO / VERIFY.

## Stage 1 historical validation

## Executed on 2026-10-24

Host: Windows 11, Windows PowerShell 5.1 and PowerShell 7. The installed Atmel
toolchain reports GCC 4.3.2 `atmel-1.2.0-(mingw32_special)`, binutils
2.19.20081016 and GNU Make 3.81. Python 3.14 ran the optional verifier.

The 32-bit legacy executables initially failed to start inside the Codex sandbox
with a Windows DLL relocation error. They ran successfully in the authorized
host execution context. Windows PowerShell 5.1 also required the process-local
`-ExecutionPolicy Bypass` invocation shown in the README; no persistent host
policy was modified. These are environment observations, not compiler failures.

Both examples were actually compiled, assembled and linked with the documented
flags, and converted to ELF, HEX, BIN and map output:

| Example | `avr32-size` text | data | bss (includes stack) | BIN bytes |
| --- | ---: | ---: | ---: | ---: |
| minimal | 600 | 0 | 1024 | 1028 |
| startup-check | 736 | 8 | 1036 | 1172 |

The 1024-byte stack is allocated `NOBITS`, not flash payload. Flash alignment gaps
account for the difference between text+data and BIN length.

Checks performed:

- `avr32-readelf`: big-endian ELF32, AVR32 machine, entry `0x80000000`.
- `avr32-objdump`: SP loads `0x8000`; EVBA loads `0x80000200`; word copy/clear
  loops precede the call to `main`; exception offsets contain self branches.
- `avr32-nm -u`: no undefined symbols. Link map contains the two project objects,
  without CRT, libc or libgcc archive members.
- Independent `tests/verify_firmware.py`: flash load segments, SRAM bounds,
  allocated NOBITS stack, HEX checksums and byte-for-byte ELF/HEX/BIN agreement.
- Nonempty `.data` (8 bytes including padding) and `.bss` (12 bytes) in the
  startup fixture; its seven initializer bytes are present at the flash LMA.
- Windows PowerShell 5.1: explicit toolchain root, environment-specified bin
  directory, invocation from another directory, checkout path with spaces.
- Repeated minimal builds produced identical ELF, HEX, BIN and map hashes in
  the same checkout with the same compiler. Cross-machine/path-independent
  debug information is not claimed.
- Invalid explicit toolchain and missing PATH toolchain fail with nonzero exit
  status and actionable messages.
- Deliberate SRAM/stack overlap, oversized flash payload and unresolved runtime
  symbol fixtures fail to link. No false success is reported by the wrapper.
- `clean.ps1 -WhatIf` preserves output; normal clean removes only the temporary
  checkout's `build` tree and leaves its source files in place.

The extra error fixtures and logs are local, ignored scratch files. The permanent
artifact verifier and startup-check example provide repeatable positive checks.

## Reproduce

```powershell
.\scripts\build.ps1
.\scripts\build.ps1 -Example startup-check
python tests/verify_firmware.py build/at32uc3b0256/minimal
python tests/verify_firmware.py build/at32uc3b0256/startup-check --startup-check
avr32-readelf -h -S -l build/at32uc3b0256/startup-check/firmware.elf
avr32-objdump -d -j .reset -j .exception_vectors build/at32uc3b0256/startup-check/firmware.elf
avr32-nm -u build/at32uc3b0256/startup-check/firmware.elf
```

The inspection commands require the toolchain's bin directory on the caller's
PATH; `build.ps1` only changes PATH within its own invocation.

## Not executed / TODO / VERIFY

- No firmware was flashed; no physical board or AVR32 simulator was run.
- Stack behavior, memory initialization and exception entry on actual silicon
  still need the [debugger fixture procedure](../examples/startup_check/README.md).
- Board-specific watchdog/fuses, oscillator wiring, bootloader coexistence,
  programming hardware and silicon errata remain to be established.
- Atmel toolchain 3.4.3.820 and Linux builds have not been tested.
- No Arduino API, peripheral behavior, timing accuracy or C++ runtime is claimed.
