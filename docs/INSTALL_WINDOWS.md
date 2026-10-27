# Install UC3Bduino on Windows

**0.8.0-dev — Experimental / Development.** Compilation and host tests do not
verify physical DFU upload, GPIO timing or USB CDC. Follow [HARDWARE_TEST](HARDWARE_TEST.md)
before relying on a board. FQBN: **`uc3bduino:avr32:uc3b0256`**.

## 1. Prerequisites and hardware configuration

- Windows 11, Windows PowerShell 5.1 or PowerShell 7; [Arduino IDE 2.x](https://www.arduino.cc/en/software/).
  Install [Arduino CLI](https://docs.arduino.cc/arduino-cli/installation/) for command-line
  checks, or supply `-CliPath` to the executable bundled with the IDE. No admin
  rights are normally needed for the sketchbook installation.
- A complete checkout or the extracted UC3Bduino development source ZIP.
  Run commands below from its root. Do not copy the platform template alone:
  the installer assembles its shared HAL, USB, startup, linker and tools.
- A board with the **verified AT32UC3B0256 marking**, factory DFU bootloader and
  correct supply/regulator/decoupling circuitry. This is a 3.3 V I/O design;
  do not connect USB's 5 V VBUS directly to MCU I/O. Check the board schematic,
  VDDIO/VDDANA/VDDIN/VDDCORE and USB supply wiring against [HARDWARE](HARDWARE.md).
- Data-capable USB cable, D+/D- and VBUS wired correctly, board-supported reset
  and DFU entry, and a verified 8/12/16 MHz OSC0 crystal for factory DFU.
- A safe GPIO for D0 (onboard LED is **not** assumed); use a series resistor for
  an external LED. Optional debug UART needs a compatible 3.3 V TTL adapter,
  common ground and the selected TX/RX route, not RS-232 or 5 V signals.

Set board parameters from the schematic, **not from an example's pin numbers**.
Without a D0 mapping `ArduinoMinimal` compiles; `Blink` intentionally fails with
a clear message. Native USB also needs authorized VID/PID and configured OSC0.
No VID/PID allocation is supplied by this project; defaults remain zero/detached.

## 2. Install the external AVR32 compiler

Use the legacy **AVR32**, not AVR 8-bit or Arm, GNU toolchain from the
[Microchip archive](https://www.microchip.com/en-us/tools-resources/archives/avr-sam-mcus).
The listed [2.4.2 Windows installer](https://ww1.microchip.com/downloads/archive/avr32-gnu-toolchain-2.4.2-setup.exe)
has a compiler family matching the tested GCC 4.3.2 baseline. The exact original
local installer has not been recovered; the newly downloaded installer is not
claimed to have been installed/tested. See [TOOLCHAIN](TOOLCHAIN.md) for versions,
sizes, legacy OS support and license limitations. Keep the complete installation.

```powershell
$env:AVR32_TOOLCHAIN_PATH = 'D:\Tools\avr32-toolchain' # your actual root or bin
```

The installer validates gcc/g++, assembler/linker, ar, objcopy and size, plus
the AVR32 target and `uc3b0256 -> ucr1` multilib. The build still requires the
vendor headers in that installation. Environment changes here affect this shell
only; the installer records an absolute bin path for the IDE.

## 3. Install the DFU tool

One optional helper fetches the pinned official x64 ZIP, verifies its size and
SHA-256, and extracts the **whole** upstream distribution, including licenses:

```powershell
.\scripts\get-dfu-tool.ps1
# Set UC3B_DFU_TOOL_PATH using the actual path printed by that command.
```

It does not run the EXE or install a USB driver. For offline use, pass
`-ArchivePath 'D:\Downloads\dfu-programmer-x64-1.1.0.zip'` and optionally a fresh
`-Destination`. Hash/size: **423421 bytes**, SHA-256
`0ab135fd9f7c54d142fdd9112f2183d82f5a362032dc3d52860a84756fe96e27`.
The hash was measured from the upstream release asset; it is not a vendor signature.
Manual [download and WinUSB driver instructions](DFU_UPLOAD.md#windows-tool-and-usb-driver)
are also available. Driver installation must target only the physical **03EB:2FF6**
DFU device, not a hub or another USB device. Do not replace the application CDC driver.

```powershell
$env:UC3B_DFU_TOOL_PATH = 'D:\Tools\dfu-programmer-1.1.0' # directory containing EXE/DLLs
```

## 4. Install the core once

Close the IDE. The basic invocation is:

```powershell
.\scripts\install.ps1
```

This installs a deliberately unconfigured core. For Blink, add `-GpioPort A|B`
and `-GpioPin` with the safe pad from your schematic. For CDC add `-Osc0Hz`,
`-Osc0Mode Crystal|External`, `-UsbVendorId` and `-UsbProductId`; factory DFU still
requires its own supported crystal hardware. Optional hardware UART uses
`-SerialRoute` and `-SerialRxBufferSize`. See [USB setup](USB_CDC.md) and
[UART routes](SERIAL.md). These are installer parameters, not board menus.

Example using **values you have already verified**:

```powershell
# Assign these variables from your schematic and your authorized USB allocation.
.\scripts\install.ps1 -SketchbookPath $sketchbook -GpioPort $gpioPort -GpioPin $gpioPin `
  -Osc0Hz $oscillatorHz -Osc0Mode Crystal -UsbVendorId $usbVid -UsbProductId $usbPid
```

For a legacy bootloader, after checking the physical MCU marking, add
`-LegacyDevice AT32UC3B0256`; otherwise uploads with an ambiguous legacy signature
are rejected. This option is not automatic device identification.

Sketchbook resolution: explicit `-SketchbookPath` wins; otherwise CLI reads
`directories.user` using explicit `-ConfigFile`, or the IDE's
`%USERPROFILE%\.arduinoIDE\arduino-cli.yaml` when present, or normal CLI defaults.
CLI environment overrides are respected. If configuration cannot be read, the
installer asks you via an error to supply the path from IDE Preferences; it never
guesses a different Documents folder. The detected path is printed before copying.
Use `-WhatIf` to validate settings without writing files.

The destination is `<sketchbook>\hardware\uc3bduino\avr32`. Existing destinations
are refused, including partial installations. For an update, close the IDE,
back up the old package and local configurations, then move that **one platform**
outside the sketchbook's hardware tree yourself, or choose a fresh sketchbook.
Reinstall with the original hardware options. No upgrade/uninstall deletes data.

Missing DFU permits compile-only installation with a warning. An explicitly
selected or PATH-discovered DFU tool must be 1.1.0. The installer pins its path
in `platform.local.txt`. If installed later, reinstall the core or set the
environment for the IDE process as explained in [DFU_UPLOAD](DFU_UPLOAD.md).

If downloaded scripts are blocked, review them, then use a one-process override:
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\install.ps1`
(append the same options). No persistent execution policy change is necessary.

## 5. Verify, upload Blink and test USB

1. Start/restart Arduino IDE 2.x. Select **Tools > Board > UC3Bduino AVR32
   (Experimental) > UC3Bduino AT32UC3B0256**.
2. Open **File > Examples > UC3BduinoExamples > Blink**, then **Verify**.
   Blink contains only GPIO and timing calls.
3. Enter factory DFU using the board's reset/boot procedure; confirm **03EB:2FF6**.
   The original B0/B1 default is PA13 low across external reset, but modified
   bootloader configuration may differ. Follow [DFU_UPLOAD](DFU_UPLOAD.md#enter-dfu-on-the-real-board).
4. Click normal **Upload** (not Upload Using Programmer / Burn Bootloader).
   No serial COM selection or programmer entry is needed for DFU. The uploader
   preserves the protected 8 KiB factory bootloader; never use a JTAG chip erase.
5. Expect D0 high/low for nominal 500 ms each. Check the pad with an instrument;
   LED polarity is board-dependent. Internal RC timing is not crystal-accurate.
6. With OSC0 and authorized USB IDs configured, open **USBSerialBlink**, Verify,
   re-enter factory DFU and Upload. Select the new **application CDC COM port**,
   then open Serial Monitor. Any monitor baud is line-coding metadata, not a
   hardware UART divisor; DTR must be asserted.
7. Expect repeated `UC3Bduino alive: ... ms`; test `USBSerialEcho` and reconnect.
   There is no automatic 1200-baud reset back into DFU.

The IDE menu sequence is documented but has not been exercised in this environment.

## CLI and diagnostics

Use the same sketchbook as the IDE. With a custom path, set it for this shell:

```powershell
$env:ARDUINO_DIRECTORIES_USER = 'D:\ArduinoSketchbook' # actual path
arduino-cli board listall UC3Bduino
arduino-cli compile --fqbn uc3bduino:avr32:uc3b0256 examples/Blink
.\scripts\diagnose.ps1
.\scripts\smoke-test.ps1
```

Diagnosis compiles `ArduinoMinimal` by default (`-SkipCompile` skips explicitly).
It checks the **installed** compiler path and DFU version; device discovery uses
Windows PnP only. Missing IDE/DFU device is informational; failed tool/platform/
compile checks exit 1, including a missing DFU tool on a compile-only setup.
Smoke tests compile installed Blink and USBSerialBlink and retain logs/artifacts
in a new temporary directory (or fresh `-OutputDirectory`). Both support
`-SketchbookPath`, `-CliPath`, `-ConfigFile`. Neither uploads, even if a board is
connected. [Troubleshooting](TROUBLESHOOTING.md) explains common failures.

For an explicit CLI upload after a successful compile:

```powershell
arduino-cli compile --fqbn uc3bduino:avr32:uc3b0256 --build-path build/Blink examples/Blink
arduino-cli upload --fqbn uc3bduino:avr32:uc3b0256 --input-dir build/Blink
```

Boards Manager installation is **not available yet**. No package index or future
release URL is advertised; see [PACKAGING](PACKAGING.md).
