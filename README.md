# UC3Bduino

**UC3Bduino 0.9.0-rc1 for Arduino IDE 2.x on Windows x64.** Arduino-compatible core for
**AT32UC3B0256**. Stable FQBN: **`uc3bduino:avr32:uc3b0256`**.
Physical DFU upload, red LED blinking, USB CDC enumeration and CDC transmission
have passed on the AT32UC3B0256 / 12 MHz reference board. Repeated automatic
upload cycles passed on revision I; Arduino IDE GUI acceptance remains pending.
SPI internal loopback passed; external SPI devices remain untested.
Wire provides a GPIO software master; external I²C communication is untested.
Preserve the factory DFU bootloader.

## Install the release candidate

Follow the [Windows Boards Manager guide](docs/INSTALL_ARDUINO_WINDOWS.md).
The Windows setup helper installs dfu-programmer 1.1.0. AVR32 GCC is a separate prerequisite.

## Install from a development checkout

1. Install Arduino IDE 2.x/CLI and the legacy AVR32 toolchain; set
   `AVR32_TOOLCHAIN_PATH` to its root or bin. [Windows setup](docs/INSTALL_WINDOWS.md)
   provides exact downloads, checksums, driver and hardware requirements.
2. Optionally run `.\scripts\get-dfu-tool.ps1` to fetch and verify dfu-programmer
   1.1.0, then set `UC3B_DFU_TOOL_PATH` to the printed executable directory.
3. Run `.\scripts\install.ps1`. The reference profile uses a 12 MHz crystal and
   D0/LED_BUILTIN on PA23, active low. The script detects the sketchbook; use
   `-SketchbookPath` to override it. Existing installs are
   preserved, not overwritten.
4. Restart IDE, select **UC3Bduino AT32UC3B0256**, open **File > Examples >
   UC3BduinoExamples > Blink**, then Verify. `.\scripts\diagnose.ps1` and
   `.\scripts\smoke-test.ps1` check the installation without flashing.
5. For explicit Upload, configure the DFU driver and enter factory DFU. Confirm
   the MCU marking before enabling `-LegacyDevice AT32UC3B0256` for legacy
   signatures. Follow the [DFU instructions](docs/DFU_UPLOAD.md).
6. For USBSerialBlink, install with verified OSC0 and your authorized VID/PID
   using the [USB configuration](docs/USB_CDC.md). Upload from DFU, select the
   application's CDC COM port and open Serial Monitor.

The reference board profile is documented in [BOARD_REFERENCE](docs/BOARD_REFERENCE.md).
USB ID allocation must be supplied locally by the developer.
Default USB IDs are zero, so SerialUSB stays detached until configured.
The source ZIP also supports the sketchbook installer.
See [packaging/version policy](docs/PACKAGING.md), [changelog](CHANGELOG.md),
[troubleshooting](docs/TROUBLESHOOTING.md) and [hardware checklist](docs/HARDWARE_TEST.md).

| Feature | Development status |
| --- | --- |
| GPIO | Implemented; explicit D0 mapping, build/host checks |
| millis/micros/delay/delayMicroseconds | Implemented; build/host checks |
| HardwareSerial / Print | Implemented; explicit UART/clock, build/host checks |
| Factory DFU upload | Protected upload/readback verified on reference hardware |
| USB CDC / SerialUSB | Enumeration and TX verified on reference hardware |
| Automatic USB upload | Opt-in 1200-baud touch / watchdog / legacy GP31; repeated reference-board cycles passed |
| analogRead(A0..A7) | Implemented; native 10 bits, build/host checks; hardware pending |
| analogReference / analogReadResolution | Not supported; external ADVREF, fixed 10-bit API |
| SPI master | Modes 0–3, transactions, byte/word/buffer transfers; internal HW loopback passed, external bus untested |
| Wire / I²C | Software GPIO master, 32-byte buffers, repeated START, bounded waits; external bus untested |
| PWM | Not implemented |

## Automatic USB upload

The opt-in Stage 8.9 path uses 1200-baud CDC plus a DTR falling edge, then a
watchdog reset into existing factory DFU. **Factory 1.0.2 requires nonvolatile
ISP_FORCE (GP31)**; after a failed upload it intentionally remains in DFU until
a successful launch. The default installation leaves this backend disabled.
See [setup, scripts, Arduino integration and recovery](docs/AUTO_UPLOAD.md).

## SPI

`#include <SPI.h>` provides `SPI.begin()`, `beginTransaction(SPISettings(...))`,
`transfer()`, `transfer16()` and `endTransaction()`. MOSI=PA14, MISO=PA25,
SCK=PA15; chip select is sketch-controlled GPIO (SS=PA16). The default 12 MHz
profile supports calculated SPI clocks up to 6 MHz. Use the SPILoopback or
SPIMaster examples and read [SPI setup, limits and tests](docs/SPI.md) first.
Foreground only; external wiring and slave devices are not hardware-validated.

## Wire / software I²C

For `Wire.begin()`, buffered writes, reads and repeated STARTs, see
[Wire / software I²C](docs/WIRE.md). SDA=PA10, SCL=PA09, external pull-ups required.
This initial implementation uses GPIO, not the native TWI controller. The
requested 1–100 kHz rate is an upper bound; no slave/ISR/400 kHz mode is provided.

## ADC Quick Start

Verify the board schematic and external ADVREF first. A0 is PA03 (64-pin
package pin 9); valid input is 0..ADVREF. Never connect a 5 V signal.
Open **UC3BduinoExamples > AnalogRead**, or use:

```cpp
void setup() { SerialUSB.begin(); }
void loop() {
    int value = analogRead(A0); // 0..1023; -1 means failure
    SerialUSB.println(value);
    delay(100);
}
```

SerialUSB needs the clock/VID/PID setup above; enumeration and TX passed on the reference board.
Both ADC examples support HardwareSerial: set `ADC_USE_USB` to `0`, configure
the [UART route and oscillator](docs/SERIAL.md), and use 115200 baud.
See [ADC reference, pin mapping, timing and potentiometer test](docs/ADC.md).
A0..A7 map to PA03..PA08/PA30/PA31. If D0 is configured on PA03, it aliases A0;
do not drive that pad as a Blink output while measuring an analog signal.

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
API subset. `analogRead()` maps A0–A7 through the shared variant and ADC HAL.
All fourteen Arduino examples compile; IDE GUI and hardware execution
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
.\scripts\install.ps1 -SketchbookPath 'D:\ArduinoSketchbook'
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
src/hal/                    clock, GPIO, timer, UART and ADC using installed headers
src/usb/                    USBB driver, EP0 device core, CDC and central config
linker/at32uc3b0256.ld        device memory layout and stack reservation
tools/targets/               device compiler selection
arduino/hardware/            local Arduino platform, core and variant template
scripts/                    Windows build, clean and Arduino installation
examples/                   three bare-metal and fourteen Arduino examples
tests/                      artifact inspection and native timing/serial/USB/ADC tests
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
- USB is a single CDC function with bounded buffering and optional automatic DFU
  reset. Reference-board enumeration and TX passed; throughput, interrupt latency
  and suspend current are unmeasured. An earlier cold-reconnect descriptor failure
  remains unresolved. USB VID/PID must be configured by the developer.
- ADC uses external ADVREF, sequential blocking conversions and foreground ownership.
  Physical accuracy and channel settling remain untested; failures return -1.
- SPI is a polling master with software chip select; no slave/DMA/ISR API.
  Wire is a foreground software master with no external bus hardware validation.
  PWM and an external-interrupt API are future stages.
  Stage 7 supplies the local installer and development source archive.

See [ROADMAP](docs/ROADMAP.md), [ARCHITECTURE](docs/ARCHITECTURE.md), and
[THIRD_PARTY](docs/THIRD_PARTY.md). Original project files use the [MIT license](LICENSE).
