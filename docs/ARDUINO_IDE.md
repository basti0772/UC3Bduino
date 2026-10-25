# Local Arduino installation (Stage 4 – HardwareSerial / UART)

The local package exposes **UC3Bduino AT32UC3B0256**, with FQBN
`uc3bduino:avr32:uc3b0256`. It provides a small C++98 core, `setup()`/`loop()`,
`pinMode(INPUT/OUTPUT/INPUT_PULLUP)`, `digitalWrite()` and `digitalRead()` over
the existing C HAL. Stage 3 adds hardware-backed time APIs and Blink/MillisTest/
MicrosPulse; see [TIMING](TIMING.md). Stage 4 adds HardwareSerial/Print and three
Serial sketches; configure their pins and clock as described in [SERIAL](SERIAL.md).
Upload and hardware execution are not tested or integrated.
See [TESTING](TESTING.md) for the distinction between CLI and GUI results.

## Install locally on Windows

Use a complete checkout of this repository and the separately installed legacy
AVR32 GNU toolchain, including `avr32-g++` and `avr32-ar`. The tested compiler is
4.3.2; 8-bit AVR and Arm toolchains do not target this chip. No compiler or ASF
is downloaded or bundled by the installer.

In Arduino IDE 2.x, check **File > Preferences > Sketchbook location**. For CLI,
run `arduino-cli config get directories.user`. A typical Windows value is
`C:\Users\<name>\Documents\Arduino`; custom/OneDrive locations may differ.
Supply that actual path explicitly:

```powershell
$env:AVR32_TOOLCHAIN_PATH = 'D:\Tools\avr32-toolchain' # replace with your installation
.\scripts\install-arduino-hardware.ps1 -SketchbookPath 'D:\ArduinoSketchbook' -WhatIf
.\scripts\install-arduino-hardware.ps1 -SketchbookPath 'D:\ArduinoSketchbook'
```

`-ToolchainPath` overrides the environment variable; root or bin paths work.
When neither is supplied, the installer finds `avr32-gcc.exe` on PATH and checks
the companion tools. It writes the chosen absolute bin path, with quoted recipe
invocations, into `platform.local.txt`. Arduino recipes do not depend on the IDE
inheriting `AVR32_TOOLCHAIN_PATH` or on shell expansion of that variable.

Installation creates `<Sketchbook>/hardware/uc3bduino/avr32/`, containing:

- `boards.txt`, `platform.txt`, and generated `platform.local.txt` / `boards.local.txt`.
- `cores/uc3bduino/Arduino.h`, `main.cpp`, `wiring_digital.cpp`, `wiring_time.cpp`.
- `Print` and `HardwareSerial` headers/implementations, plus variant `serial_config.h`.
- Exact copies of `src/hal/*` under the core's `hal/` and the existing startup
  under `startup/`, plus the existing linker script under `system/`.
- The `uc3b0256` variant, project license and external header notices.
- `shared-sources.json`, recording original paths and SHA-256 hashes.

These are installation snapshots, not a second source of truth. Change shared
code in the repository and reinstall for a new version. The installer refuses
an existing destination, even if created by a previous run; it never deletes or
overwrites an existing package. Back up/move it yourself before reinstalling,
or use a new isolated sketchbook. `-WhatIf` performs no installation. No admin
rights are needed for a user-owned sketchbook. If local script execution is
restricted, use the README's one-process PowerShell execution-policy override.

## Arduino IDE 2.x procedure

1. Install into the sketchbook actually selected in Preferences, then restart IDE.
2. Open `examples/ArduinoMinimal/ArduinoMinimal.ino` from this checkout.
3. Select **Tools > Board > UC3Bduino AVR32 (compilation smoke test) >
   UC3Bduino AT32UC3B0256**. Alternatively use the board selector's other-board
   dialog. A port is not needed for compilation.
4. Click **Verify**. Expected artifacts are `<SketchName>.ino.elf`, `.hex`, `.bin`
   and `.map` in Arduino's build directory; enable verbose compilation output to
   locate it. CLI can supply an explicit build path as below.
5. With verified serial/clock options installed, open SerialHello, SerialMillis
   or SerialEcho and Verify. Arduino.h exposes Serial automatically. These three
   examples were compiled by CLI; the IDE GUI procedure remains NOT TESTED.

This is the documented IDE workflow, not a claim of a GUI test. The CLI uses
the same package recipes. There is no upload recipe, fake programmer, port
auto-detection for this board, debugger integration or Boards Manager index.
Do not use Upload as the acceptance test for this stage.

## Arduino CLI

CLI and IDE must use the same sketchbook to see the same local package. A
temporary YAML file avoids changing global CLI configuration:

```yaml
directories:
  user: D:/ArduinoSketchbook
```

Save it as `arduino-local.yaml` outside tracked source, then run from the repo:

```powershell
arduino-cli board listall UC3Bduino --config-file arduino-local.yaml
arduino-cli compile --config-file arduino-local.yaml --fqbn uc3bduino:avr32:uc3b0256 --build-path build/arduino/minimal examples/ArduinoMinimal
python tests/verify_firmware.py build/arduino/minimal --basename ArduinoMinimal.ino
```

If CLI's `directories.user` already points to the chosen sketchbook, omit
`--config-file`. Normal Arduino CLI prerequisites (indexes and builtin sketch
preprocessor/ctags tools) must be installed. Use Arduino's normal CLI setup for
a fresh installation. No AVR8 board platform is required by UC3Bduino.

## Configure the one logical GPIO test slot

There is intentionally **no `LED_BUILTIN`**. `D0` is logical slot 0 in a
one-entry variant table, not a connector label or a physical package pin number.
The default entry is invalid, so the minimal sketch builds without selecting any
pad. The DigitalOutput sketch explicitly fails compilation until configured.

Choose a free pad from the real board schematic. Install a fresh package with
both `-GpioPort A|B` and `-GpioPin number`, or edit the installed variant's
`test_pin_config.h`. Set `UC3BDUINO_TEST_PIN_CONFIGURED` to 1, its PORT to 0
(PA) or 1 (PB), and BIT to the GPIO bit number. PA03–PA31 and PB00–PB11 are
accepted; PA00–PA02 are excluded to preserve the normal JTAG pins.

For a **compilation-only example**, assuming a fresh destination:

```powershell
.\scripts\install-arduino-hardware.ps1 -SketchbookPath 'D:\ArduinoGpioTest' -GpioPort A -GpioPin 3
# Point the CLI YAML user directory at D:/ArduinoGpioTest, then:
arduino-cli compile --config-file arduino-gpio.yaml --fqbn uc3bduino:avr32:uc3b0256 --build-path build/arduino/digital examples/DigitalOutput
python tests/verify_firmware.py build/arduino/digital --basename DigitalOutput.ino
```

PA03 here is not a verified LED, available connector or safe board output.
`DigitalOutput` rapidly alternates HIGH/LOW for a scope or logic analyzer; it
has no delay. Restart the IDE after changing installed files; use CLI `--clean`
after configuration/source changes if reusing a build directory.

## Architecture and runtime limits

`avr32` names the actual processor architecture. The vendor namespace
`uc3bduino` distinguishes this package; the FQBN was discovered by the installed
CLI. The inspected official Arduino package index has no `avr32` entry (it does
contain `avr` and `arc32`, which are different architectures). This does not
claim no third-party project uses the same CPU architecture name. Libraries
marked `avr` are not automatically AVR32-compatible.

C files remain C99; sketches/core wrappers use C++98 without exceptions, RTTI,
STL, heap or a linked C/C++ runtime. HAL declarations have C linkage in C++.
The original reset/linker code is used, with `_start` explicitly extracted from
`core.a`. Nontrivial global constructors/destructors remain unsupported and the
linker rejects their initialization sections. Arduino prototype generation is
provided by the actual CLI, not by a replacement .ino parser.

`pinMode(OUTPUT)` starts LOW; INPUT disables the pull-up, INPUT_PULLUP enables
it. Invalid logical pins cause no GPIO access; `digitalRead()` returns LOW for
invalid/unconfigured pins. Unsupported modes are ignored. Nonzero writes mean
HIGH, but writing HIGH to an input only changes its latch: use INPUT_PULLUP
explicitly. These are limited smoke-test semantics, not full Arduino parity.

`F_CPU` and the HAL getters report nominal 115200 Hz by default, or the explicitly
configured OSC0 frequency. Arduino init enables the TC0 timer interrupt; long
interrupt masking loses time and may overrun Serial RX. Serial supports the
subset documented in SERIAL.md; ADC/PWM, buses and USB remain unimplemented.
Timing limits are documented in TIMING.md. Size reporting includes the reserved 1 KiB stack in SRAM usage;
program section totals omit alignment gaps, so use the BIN span/map and linker
checks for flash occupancy. Full-flash layout requires separate bootloader/fuse
and board verification before any external programming.

## Primary references

- [Arduino platform specification](https://docs.arduino.cc/arduino-cli/platform-specification/): manual layout, architecture names, recipes and property overrides.
- [Sketch build process](https://docs.arduino.cc/arduino-cli/sketch-build-process/): preprocessing, core archive and C/C++ linkage.
- [Install a custom core](https://support.arduino.cc/hc/en-us/articles/360021232160-How-to-install-and-use-a-custom-core-version-in-the-IDE): sketchbook hardware packages.
- [Arduino file locations](https://support.arduino.cc/hc/en-us/articles/4415103213714-Find-sketches-libraries-board-cores-and-other-files-on-your-computer): IDE 2 configuration and sketchbook locations.

Consulted 2026-10-25; package-specific behavior above is defined by this project.
