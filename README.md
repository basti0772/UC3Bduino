# UC3Bduino

Arduino-compatible core for Atmel AVR32 UC3B microcontrollers.

**Stage 6 – USB CDC / SerialUSB**

Native `SerialUSB` adds USB CDC ACM using the integrated device controller,
EP0 enumeration and static RX/TX buffers. `Serial` remains the hardware USART.
USB requires verified 8/12/16 MHz OSC0 and authorized developer-configured VID/PID;
the default unconfigured identity never attaches. **Hardware enumeration/COM
ports are not tested.** Stage 5's factory DFU upload and protected 8 KiB bootloader
region remain in place. **DO NOT overwrite the factory USB DFU bootloader.**

## First test

1. Install UC3Bduino into the actual IDE sketchbook, configuring OSC0, authorized
   VID/PID and safe D0 mapping as described in [USB setup](docs/USB_CDC.md).
2. Configure dfu-programmer 1.1.0; check the MCU marking for a legacy target.
3. Select **UC3Bduino AT32UC3B0256** and open `USBSerialBlink`.
4. Click **Verify**, enter factory DFU mode and click **Upload**.
5. Wait for the application CDC COM port; select it and open Arduino Serial Monitor.
6. Expect repeated `UC3Bduino alive: ... ms` while D0 toggles at nominal 1 Hz.
   Test `USBSerialEcho` by sending text back to the board.

See [DFU setup, reset/pin sequence and troubleshooting](docs/DFU_UPLOAD.md)
and [local IDE installation](docs/ARDUINO_IDE.md) for exact commands. A DFU device
does not need a COM port. The factory bootloader requires supported OSC0 hardware.

## Core status

Arduino CLI now recognizes **UC3Bduino AT32UC3B0256** and builds the supplied
`.ino` sketches to ELF/HEX/BIN/map files. The minimal C++98 core implements
`setup()`/`loop()`, `pinMode()`, `digitalWrite()` and `digitalRead()` over the
existing clock/GPIO HAL. TC0 now supplies `millis()`, `micros()`, `delay()` and
`delayMicroseconds()`; see [timing and limits](docs/TIMING.md). One `Serial`
object now provides polling TX, interrupt RX and decimal `print/println`,
including `Serial.println(millis())`. See [Serial setup](docs/SERIAL.md) for
explicit pin/clock selection. `SerialUSB` reuses the same Print implementation;
see [CDC API, configuration and limits](docs/USB_CDC.md). This is a limited Arduino
API subset. All twelve Arduino examples compile; IDE GUI and hardware execution
remain untested.

Both build paths use the same reset code, memory layout and C HAL for
**AT32UC3B0256** only. Startup initializes the stack, copies `.data`, clears
`.bss`, and calls `main()`. The clock defaults to undivided internal RC;
board-verified OSC0 can be explicitly selected for useful UART rates. No operating
system, C library or Atmel Software Framework (ASF) is linked.

## Requirements

- Windows 11 with Windows PowerShell 5.1 or PowerShell 7.
- A **legacy Atmel AVR32 GNU toolchain**. The tested compiler is
  `avr32-gcc 4.3.2 (atmel-1.2.0-(mingw32_special))`, with binutils
  `2.19.20081016`. Use AVR32 tools, not the similarly named 8-bit AVR tools.
- Companion `avr32-g++` and `avr32-ar` for the Arduino build. Arduino CLI
  1.5.2-rc.1 was tested; the Arduino IDE 2.x installation procedure is documented.
- For bare-metal builds: GNU Make 3.81 or newer (`make.exe` or `mingw32-make.exe`). The tested historical
  toolchain already includes Make 3.81.
- Optional: Python 3.8+ for artifact validation; it is not needed to build.
- For upload: dfu-programmer 1.1.0 x64 and a usable WinUSB driver for 03EB:2FF6;
  see [DFU_UPLOAD](docs/DFU_UPLOAD.md). No Python is needed for upload.

Obtain the toolchain separately from the
[Microchip archive](https://www.microchip.com/en-us/tools-resources/archives/avr-sam-mcus).
No compiler binaries, vendor headers, or third-party runtime sources are bundled.
See [toolchain details and flags](docs/TOOLCHAIN.md).

## Arduino compilation on Windows

Use the actual sketchbook path from IDE Preferences or
`arduino-cli config get directories.user`. Install into a fresh destination:

```powershell
$env:AVR32_TOOLCHAIN_PATH = 'D:\Tools\avr32-toolchain' # replace with your installation
.\scripts\install-arduino-hardware.ps1 -SketchbookPath 'D:\ArduinoSketchbook'
```

The installer copies the platform template and unchanged shared HAL/startup/linker
files. It records their hashes and writes a local compiler-path override; it
refuses to overwrite an existing package. Restart Arduino IDE, select
**UC3Bduino AT32UC3B0256**, open `examples/ArduinoMinimal/ArduinoMinimal.ino`
and choose **Verify**. For CLI using that same sketchbook:

```powershell
arduino-cli board listall UC3Bduino
arduino-cli compile --fqbn uc3bduino:avr32:uc3b0256 --build-path build/arduino/minimal examples/ArduinoMinimal
python tests/verify_firmware.py build/arduino/minimal --basename ArduinoMinimal.ino
```

See [ARDUINO_IDE](docs/ARDUINO_IDE.md) for isolated CLI configuration, exact
installation layout and the `DigitalOutput` example. Its logical `D0` slot needs
an explicit GPIO mapping; no LED or full board pinout is assumed. The
[DFU uploader](docs/DFU_UPLOAD.md) uses the same installed scripts for CLI/IDE.
The [test matrix](docs/TESTING.md) separates executed builds from
the documented IDE procedure and pending hardware tests.

For Serial, select a route and actual oscillator using the installer options
in [SERIAL](docs/SERIAL.md). There is no assumed board mapping, and the default
RC clock cannot generate 115200 baud. `SerialHello`, `SerialMillis` and
`SerialEcho` exercise output, timer integration and receive/echo respectively.

## Bare-metal build on Windows

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
| `firmware.bin` | Raw application image starting at `0x80002000`; upload uses HEX |
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
The wrapper rebuilds all four objects each time, avoiding stale flags or toolchains.
Build output and locally downloaded tools are excluded from Git.

For the GPIO smoke test, choose a free pad from your actual board schematic.
There is deliberately no default LED pin:

```powershell
# Compilation example only: PA03 is NOT an assumed board LED or safe output.
.\scripts\build.ps1 -Example gpio-toggle -GpioPort A -GpioPin 3
python tests/verify_firmware.py build/at32uc3b0256/gpio-toggle
```

Missing or invalid pin options fail clearly. Optional `-BusyWaitIterations`
changes the temporary loop count, not a duration in milliseconds. See the
[smoke-test procedure](examples/gpio_toggle/README.md), [GPIO contract](docs/GPIO.md)
and [clock configuration](docs/CLOCK.md). CPU, HSB, PBA and PBB are nominally
115200 Hz; the internal RC oscillator is not a precision time reference.

## Layout

```text
src/main.c                   minimal program
src/startup/uc3_startup.S     original UC startup and exception traps
src/hal/                    clock, GPIO, timer and UART using installed headers
src/usb/                    USBB driver, EP0 device core, CDC and central config
linker/at32uc3b0256.ld        device memory layout and stack reservation
tools/targets/               device compiler selection
arduino/hardware/            local Arduino platform, core and variant template
scripts/                    Windows build, clean and Arduino installation
examples/                   three bare-metal and twelve Arduino examples
tests/                      artifact inspection and native timing/serial/USB tests
docs/                       hardware sources, design, roadmap, validation
```

The official Atmel device headers are supplied by the installed toolchain.
See [versions, hashes and retained notices](docs/THIRD_PARTY.md).

## Current limitations

- **CLI builds verified; IDE GUI, hardware execution and flashing untested.** See
  [TESTING](docs/TESTING.md), the earlier [validation record](docs/VALIDATION.md)
  and [hardware assumptions](docs/HARDWARE.md).
- DFU upload assumes the intact factory bootloader and BOOTPROT=3. Legacy
  signatures need explicit physical MCU confirmation; unknown devices abort.
- Internal RC remains the default; optional OSC0 accepts an explicit 0.4..16 MHz
  crystal or external clock. Board crystal, pin wiring and watchdog/fuses are
  unknown. USB reserves PLL1/GCLK3; no complete board pin mapping, general INTC
  dispatch, watchdog servicing or calibrated timing is supplied.
- Arduino timer init enables interrupts after configuring TC0 and its vector.
  Bare-metal examples keep interrupts masked. Exceptions halt at trap entries.
- C99 and C++98 with legacy AVR32-GCC 4.3.2. Global C++ constructors,
  exceptions, RTTI, heap, libc, and syscalls
  are not supported. Additional UC3B variants require explicit validation.
- Default timing uses nominal RC frequency: 1 ms returned granularity for millis,
  69.44 us counter resolution for micros (finer with OSC0). Timer service must occur within
  each 10 ms period. Timing accuracy and API latency are not hardware-verified.
- Serial supports one selected USART, 8N1, polling TX and 63 usable RX bytes
  by default. Pin/clock configuration is required; RX service latency is unmeasured.
  Print supports decimal integers/text, with no full Stream/virtual Print ABI.
- USB is a single CDC function with bounded buffering and no automatic DFU reset.
  Physical enumeration, throughput, interrupt latency and suspend current need
  board validation. USB VID/PID must be configured by the developer.
- No SPI, Wire, ADC, PWM, external-interrupt API or Boards Manager distribution.
  Stage 7 is packaging / easy installation.

See [ROADMAP](docs/ROADMAP.md), [ARCHITECTURE](docs/ARCHITECTURE.md), and
[THIRD_PARTY](docs/THIRD_PARTY.md). Original project files use the [MIT license](LICENSE).
