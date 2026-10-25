# Test record

## Stage 3 – Arduino timing foundation

Executed 2026-10-25 with the same AVR32 GCC/G++ 4.3.2, binutils 2.19.20081016,
Arduino CLI 1.5.2-rc.1 and Windows PowerShell 5.1 baseline recorded below.
Native logic tests used MSVC 14.51.36231, `/W4 /WX`. No MCU or simulator ran.

| Check | Status | Scope |
| --- | --- | --- |
| ArduinoMinimal, DigitalOutput, Blink, MillisTest, MicrosPulse | PASS | Actual CLI compile/link and independent ELF/HEX/BIN/map checks |
| Bare-metal minimal, gpio-toggle, startup-check | PASS | Rebuilt and verified; prior sizes unchanged |
| Timer snapshot races and wrap | PASS | Real C implementation; scripted CV/SR events, pending flags, no double-count, GM preservation |
| Timer conversion bounds | PASS | All 144 current-clock CV values and 65535 maximum-period positions against 64-bit test oracles |
| Clock/init rejection | PASS | Unmasked/repeated init, zero/nondivisible/too-fast PBA rejected |
| Wait arithmetic | PASS | Real C++ wrappers: zero, wrap boundaries, chunk boundaries and UINT32_MAX durations |
| Vector/ISR machine code | PASS | 0x80000304 branches to strong timer ISR; status acknowledgment, two epoch adds and rete; no ISR division |
| Snapshot machine code | PASS | GM mask plus two NOPs, CV before SR, conditional reread, prior-mask restoration and native divu |
| IDE GUI | NOT TESTED | CLI tests only |
| Upload / hardware timing / ISR latency | NOT TESTED | No physical execution, pulse measurements or masking stress run |

| Arduino sketch | text | data | bss including 1 KiB stack | BIN bytes |
| --- | ---: | ---: | ---: | ---: |
| ArduinoMinimal | 952 | 0 | 1036 | 1376 |
| DigitalOutput | 1576 | 0 | 1036 | 2000 |
| Blink | 1788 | 0 | 1036 | 2212 |
| MillisTest | 1804 | 0 | 1044 | 2228 |
| MicrosPulse | 1824 | 0 | 1040 | 2248 |

Compared with Stage 2.5 ArduinoMinimal, timing initialization/ISR adds **252
bytes text/BIN and 12 bytes static RAM**. Unused read/delay APIs are removed by
section GC in that sketch. Bare-metal sizes remain 680/1088/740 text for
minimal/gpio-toggle/startup-check. No runtime library was added.

Validation installed a fresh isolated `.scratch/stage03/sketchbook` with explicit
PA03 for compilation only. Its CLI YAML used that user directory and the existing
Arduino15 data directory. For each of the five sketch names, the actual pattern was:

```powershell
arduino-cli compile --config-file .scratch/stage03/arduino-cli.yaml --fqbn uc3bduino:avr32:uc3b0256 --build-path .scratch/stage03/build-Blink --verbose examples/Blink
python tests/verify_firmware.py .scratch/stage03/build-Blink --basename Blink.ino
```

Native tests can be reproduced from a Visual Studio developer command prompt
(create the ignored output directory first):

```bat
cl /nologo /W4 /WX /TC /std:c11 tests\timing_snapshot_test.c /Fobuild\timing_snapshot_test.obj /Febuild\timing_snapshot_test.exe
build\timing_snapshot_test.exe
cl /nologo /W4 /WX /EHsc /I src /I arduino\hardware\uc3bduino\avr32\cores\uc3bduino /I arduino\hardware\uc3bduino\avr32\variants\uc3b0256 tests\timing_delay_test.cpp /Fobuild\timing_delay_test.obj /Febuild\timing_delay_test.exe
build\timing_delay_test.exe
```

The tests replace only hardware observations/clock advancement, not the snapshot
or wait algorithms. They do not prove INTC/MMIO timing, oscillator accuracy or
interrupt latency. See [TIMING](TIMING.md) for the sub-10-ms service requirement,
quantization, nominal scope targets and pending on-device checks.

## Stage 2.5 – Arduino IDE compilation smoke test (historical)

Executed on **2026-10-25**, Windows 11. The installed Arduino CLI reports
**1.5.2-rc.1**, commit `fef6e48df`, build date 2026-07-23. The AVR32 toolchain
is GCC/G++ **4.3.2** (`atmel-1.2.0-(mingw32_special)`), binutils
**2.19.20081016** and Make **3.81**. Installation/build scripts ran with Windows
PowerShell 5.1. No replacement compiler was downloaded. This records the
installed CLI release candidate, not a claim about every CLI/IDE release.

## Matrix

| Check | Result | Evidence / scope |
| --- | --- | --- |
| Package structure and recipes | PASS | Reviewed against official Arduino platform specification; exercised through CLI |
| Arduino CLI board discovery | PASS | `UC3Bduino AT32UC3B0256`, FQBN `uc3bduino:avr32:uc3b0256` |
| ArduinoMinimal sketch compile | PASS | Preprocess, C++/C/assembly, core archive, link, ELF/HEX/BIN/map |
| DigitalOutput sketch compile | PASS | Explicit D0 = PA03 for compilation only; no LED or safe board pad inferred |
| GPIO API / generated prototype compile | PASS | Scratch sketch uses INPUT, OUTPUT, INPUT_PULLUP, reads, writes, invalid indices and a later-defined function |
| Unconfigured DigitalOutput rejection | PASS | Nonzero build status with explicit missing-pin configuration error |
| Unsupported global constructor rejection | PASS | Link fails with `Unsupported C++/TLS/PIC runtime` |
| Arduino artifact verification | PASS | Independent verifier on both examples and the API fixture |
| Bare-metal minimal build | PASS | Rebuilt and artifact-verified after C linkage header changes |
| Bare-metal GPIO build | PASS | PA03 compilation only; rebuilt and artifact-verified |
| Bare-metal startup-check build | PASS | Rebuilt; initialized data and artifacts verified without execution |
| Installer / shared-source identity | PASS | Fresh install, 11 copied files match source bytes and recorded SHA-256 |
| Installer non-destructive checks | PASS | Existing target rejected unchanged; WhatIf creates nothing; invalid/partial pin options rejected |
| Paths with spaces | PASS | Actual compiler path, configured sketchbook and DigitalOutput build path contain spaces |
| Arduino IDE 2.x board discovery | NOT TESTED | GUI was not launched; procedure documented in ARDUINO_IDE.md |
| Arduino IDE 2.x Verify | NOT TESTED | Successful CLI build does not substitute for a GUI observation |
| Integrated hardware upload | NOT AVAILABLE | No upload recipe, protocol or programmer integration implemented |
| External upload / reset execution | NOT TESTED | No programming or simulator run performed |
| Hardware GPIO / pull-up / timing | NOT TESTED | No physical electrical behavior measured |

Runtime invalid-pin behavior was reviewed in source, not executed on a host or
MCU. API-fixture compilation demonstrates linkage and prototype generation,
not runtime GPIO correctness. Interrupts remain globally masked.

## Artifact results

| Build | `avr32-size` text | data | bss, including stack | BIN bytes |
| --- | ---: | ---: | ---: | ---: |
| ArduinoMinimal | 700 | 0 | 1024 | 1124 |
| DigitalOutput, D0 = PA03 | 1324 | 0 | 1024 | 1748 |
| GpioApiCheck scratch fixture | 1544 | 0 | 1028 | 1968 |
| Bare-metal minimal | 680 | 0 | 1024 | 1104 |
| Bare-metal gpio-toggle, PA03 | 1088 | 0 | 1044 | 1512 |
| Bare-metal startup-check | 740 | 8 | 1036 | 1172 |

The 1 KiB stack is NOBITS SRAM, not flash payload. Alignment gaps explain the
difference between section totals and BIN length. The verifier checks big-endian
AVR32 ELF, reset entry, flash load sections, SRAM/stack bounds, undefined globals,
HEX checksums and byte-for-byte agreement of ELF/HEX/BIN; the map must exist.
Its new optional `--basename` handles Arduino's sketch-specific filenames while
preserving `firmware` as the bare-metal default.

Symbol and disassembly inspection of DigitalOutput confirms `_start`, C-linkage
main/HAL/wrappers and C++ setup/loop. The main call sequence targets
`uc3b_clock_init`, then setup once, then loop repeatedly (init was inlined).
`avr32-nm -u` reports no undefined symbols. The link uses the original reset and
memory layout, with no libc/libgcc/CRT additions. The first minimal compile
exposed GCC 4.3's return warning for the freestanding C++ main; an explicit
`return 0` after the endless loop resolved it while retaining `-Werror`.

## Commands actually executed

Validation used isolated sketchbooks inside ignored `.scratch/stage025`, leaving
the normal Arduino sketchbook unchanged. The toolchain root was
`C:\Program Files (x86)\Atmel\AVR Tools\AVR32 Toolchain`. It was supplied through
`-ToolchainPath` to the installer. One installation left D0 unconfigured; the
other used `-GpioPort A -GpioPin 3` and a sketchbook named `sketchbook configured`.

The two local CLI YAML files selected those sketchbooks via `directories.user`,
the existing Arduino data installation via
`directories.data: C:/Users/admin/AppData/Local/Arduino15`, and scratch downloads.
Existing CLI indexes and builtin ctags were used. Host execution was necessary
for the legacy 32-bit tools, as in the earlier [validation record](VALIDATION.md).

From the repository, with those local configurations:

```powershell
arduino-cli board listall UC3Bduino --config-file .scratch/stage025/arduino-cli-configured.yaml
arduino-cli compile --config-file .scratch/stage025/arduino-cli.yaml --fqbn uc3bduino:avr32:uc3b0256 --build-path .scratch/stage025/build-minimal --verbose examples/ArduinoMinimal
arduino-cli compile --config-file .scratch/stage025/arduino-cli-configured.yaml --fqbn uc3bduino:avr32:uc3b0256 --build-path '.scratch/stage025/build digital' --verbose examples/DigitalOutput
python tests/verify_firmware.py .scratch/stage025/build-minimal --basename ArduinoMinimal.ino
python tests/verify_firmware.py '.scratch/stage025/build digital' --basename DigitalOutput.ino
```

The three bare-metal examples were rebuilt with `scripts/build.ps1`, explicit
`-ToolchainPath` and `-Example minimal`, `startup-check`, or `gpio-toggle`
(`-GpioPort A -GpioPin 3` for GPIO), then verified with the existing commands in
[VALIDATION](VALIDATION.md). Build/error logs and temporary sketches are local
ignored scratch evidence, not required package contents.

For a new machine, use the portable installation/configuration steps in
[ARDUINO_IDE](ARDUINO_IDE.md) instead of these machine-specific scratch paths.
Repeat the unconfigured GPIO failure before selecting a real board pad. A
hardware acceptance run must separately verify reset, clock registers, actual
GPIO output/input and pull-ups using [GPIO](GPIO.md) and [HARDWARE](HARDWARE.md).
IDE GUI observations and hardware results must be recorded only after execution.
