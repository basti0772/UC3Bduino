# Test record

## Stage 4 – HardwareSerial / UART

Executed 2026-10-25 with AVR32 GCC/G++ 4.3.2, binutils 2.19.20081016,
Arduino CLI 1.5.2-rc.1, Windows PowerShell 5.1 and native MSVC 14.51.36231.
Target builds use C99/C++98 and `-Wall -Wextra -Werror`; native tests use
`/W4 /WX`. No board, simulator, terminal adapter or oscilloscope was used.

| Check | Status | Evidence / scope |
| --- | --- | --- |
| SerialHello compile | PASS | CLI preprocess, compile/archive/link, ELF/HEX/BIN/map verification |
| SerialMillis compile | PASS | Serial.println(millis()) + delay(1000), artifact verification |
| SerialEcho compile | PASS | available/read/write integration, artifact verification |
| ArduinoMinimal, DigitalOutput, Blink, MillisTest, MicrosPulse | PASS | All five regressions compile and pass artifact checks with explicit 12 MHz OSC0 |
| Default RC ArduinoMinimal, Blink, SerialHello | PASS | Fresh install without Serial route or OSC0 options; unconfigured Serial is an intentional inactive runtime configuration |
| Bare-metal minimal, gpio-toggle, startup-check | PASS | Rebuilt, sizes unchanged, independent artifact checks |
| UART baud calculation | PASS | Five required rates at 12 MHz; zero/overflow/divider boundaries, exact +/-2% acceptance boundary, rational error oracle across >15000 rates |
| RX buffer test (host) | PASS | Real HAL C logic at sizes 2/64/256; full/drop-new, wrap, peek, binary 0/255, 30000 randomized producer/consumer operations per size against reference queue |
| RX error/lifecycle logic (host) | PASS | OVRE/FRAME/PARE separately and together, errors without RXRDY, drain/reset, empty reads, repeated/invalid begin, prior GM state preserved |
| TX flush semantics (host) | PASS | Scripted TXRDY then later TXEMPTY; flush waits for latter and preserves RX |
| Print / HardwareSerial API (host) | PASS | Real C++ implementation, decimal signed/unsigned limits, LONG_MIN, zero, CRLF, partial writes, binary bytes, begin/end and invalid format |
| Existing timing native tests | PASS | Snapshot races, pending compares, rollover, conversions, delay boundaries unchanged |
| ISR/vector disassembly | PASS | UART at 0x80000308 targets strong ISR; RHR drain, ring/error handling, CSR acknowledgment + three NOPs, saved registers and rete; no calls/division in ISR |
| Runtime dependencies | PASS | No undefined globals, constructor sections, heap, sprintf, libc/libgcc/CRT; Serial is a trivial BSS object |
| OSC0 branches | PASS | External/crystal compile at 0.4/0.9/3/8/16 MHz, all gain selection branches covered |
| Installer validation | PASS | Ten invalid clock/pin/route/buffer combinations rejected before writes; WhatIf and existing-target refusal checked; case-insensitive route normalized |
| Installer shared sources | PASS | 18 files checked against SHA-256 manifest and canonical source bytes in fresh installs |
| Arduino IDE Verify / Serial recognition in GUI | NOT TESTED | Procedure documented, no GUI run |
| UART TX hardware | NOT TESTED | No transmitted waveform or terminal output observed |
| UART RX hardware | NOT TESTED | No physical echo/error recovery observed |
| 115200 baud test (hardware) | NOT TESTED | Calculation/build pass only; no bit-time measurement |
| 9600/19200/38400/57600 hardware | NOT TESTED | Calculation pass only |
| RX buffer test (hardware) / sustained traffic | NOT TESTED | No on-device overflow/latency/throughput measurements |
| OSC0 startup / timing under RX load | NOT TESTED | Board clock, silicon revision and wiring remain unknown |
| Integrated upload | NOT AVAILABLE | No upload implementation in this stage |

The UART host test replaces only the private hardware adapter; its ring and
baud logic are the production C implementation. The Print test replaces the
UART transport, not integer formatting or HardwareSerial. These tests do not
simulate AVR32 interrupts, MMIO propagation or oscillator behavior.

The configured build fixture uses **U0C, TX PB11 / RX PB10, function C,
12 MHz crystal OSC0, D0=PA03**, solely for compilation. This does not establish
real board wiring. The installer rejects U0A with OSC0 because PA18 is shared.
The required five rates calculate to +0.160256% nominal error in this fixture;
see [SERIAL](SERIAL.md) for exact formula and rounded actual rates.

| Arduino sketch (12 MHz fixture) | text | data | bss including 1 KiB stack | BIN bytes |
| --- | ---: | ---: | ---: | ---: |
| ArduinoMinimal | 996 | 0 | 1036 | 1420 |
| DigitalOutput | 1620 | 0 | 1036 | 2044 |
| Blink | 1832 | 0 | 1036 | 2256 |
| MillisTest | 1848 | 0 | 1044 | 2272 |
| MicrosPulse | 1868 | 0 | 1040 | 2292 |
| SerialHello | 2276 | 0 | 1120 | 2700 |
| SerialMillis | 2588 | 0 | 1120 | 3012 |
| SerialEcho | 2324 | 0 | 1120 | 2748 |

SerialHello/SerialMillis use **96 bytes of static RAM plus the 1024-byte stack
reservation**. This includes the 64-byte RX array, indices, route/state, Serial
callback and the existing timer state. No libc components are linked. Stack
high-water use with nested interrupts remains unmeasured. Text totals omit
alignment gaps; BIN size is the complete flash span. With default RC,
ArduinoMinimal remains 952/0/1036 and Blink 1788/0/1036 (text/data/bss), identical
to Stage 3; bare-metal totals also remain unchanged.

For reproduction, install a fresh explicit fixture following SERIAL.md, point
CLI at that sketchbook, then repeat this pattern for the eight example names:

```powershell
arduino-cli compile --config-file <your-cli.yaml> --fqbn uc3bduino:avr32:uc3b0256 --build-path build/serial-hello --verbose examples/SerialHello
python tests/verify_firmware.py build/serial-hello --basename SerialHello.ino
avr32-size build/serial-hello/SerialHello.ino.elf
```

In a Visual Studio developer command prompt, create `build` if needed, then:

```bat
cl /nologo /W4 /WX /TC /std:c11 tests\serial_uart_test.c /Fobuild\serial_uart_test.obj /Febuild\serial_uart_test.exe
build\serial_uart_test.exe
cl /nologo /W4 /WX /EHsc /I src /I arduino\hardware\uc3bduino\avr32\cores\uc3bduino /I arduino\hardware\uc3bduino\avr32\variants\uc3b0256 tests\serial_print_test.cpp /Fobuild\serial_print_test.obj /Febuild\serial_print_test.exe
build\serial_print_test.exe
```

Repeat the C test with `/DSERIAL_RX_BUFFER_SIZE=2` and `=256`. Its default is 64.
The Print test expects the target's 32-bit long ABI, also provided by MSVC;
an LP64 host intentionally fails the width assertion. Timing test commands
remain below. Physical acceptance procedure is in SERIAL.md; all such results
are still pending.

## Stage 3 – Arduino timing foundation (historical)

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
