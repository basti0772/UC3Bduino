# Stage 5 – Arduino USB DFU upload

**DO NOT overwrite the factory USB DFU bootloader.**

This Windows path targets an **AT32UC3B0256 with the existing Atmel factory
bootloader**. Hardware upload, readback and Blink execution still require a real
board; see [TESTING](TESTING.md). No bootloader binary, USB CDC stack or driver
installer is supplied. Check the actual MCU marking and board schematic first.

## Bootloader and memory contract

| Region | Stage 5 policy |
| --- | --- |
| `0x80000000..0x80001FFF` | 8 KiB factory bootloader; never in application images |
| `0x80002000..0x8003FFFF` | 248 KiB application; `_start` at `0x80002000` |
| `0x00000000..0x00000003` | Reserved for the bootloader's warm-reset key |
| `0x00000004..0x00007FFF` | Application SRAM, including the existing 1 KiB stack |
| `0x80800000..0x808001FF` | User page; never supplied as upload data |
| General-purpose fuses/security | No explicit writes by our scripts |

Both builds use `linker/at32uc3b0256.ld`; the Arduino installer copies that exact
file. There is no reset trampoline at flash address zero. Rebuild old Stage 1–4
images: the uploader rejects their bootloader-overlapping addresses.

The reset fetch remains at `0x80000000`; the bootloader dispatches to the
application. BOOTPROT must read **3**, protecting 8 KiB. The DFU erase operation
preserves this protected boot area. This is not a JTAG chip erase. Our wrapper
checks protection before erase and never tries to repair fuses. These layout,
entry and protection rules come from [AVR32784, sections 5–6 and 7.4.2](https://www.microchip.com/content/dam/mchp/documents/OTH/ApplicationNotes/ApplicationNotes/doc7745.pdf).

## Enter DFU on the real board

For the original B0/B1 configuration (`0x929E0D6B`), the ISP condition is GPIO
13, **PA13 low during external reset**. Hold PA13 low, assert RESET_N low,
release RESET_N while holding PA13, wait for USB enumeration, then release PA13.
If absent on the board, provide an accessible momentary switch from PA13 to GND
and a pull-up to the board's I/O supply, plus a RESET_N switch wired according
to the board reset circuit. Verify pad/package numbering and that nothing
else drives the pad. These are signal names, not assumed board button labels.
The default pin is configurable; a previously modified user page may differ.

Factory ISP requires a working **8, 12 or 16 MHz OSC0 crystal**, USB D+/D−,
USB supply/VBUS wiring and a data-capable cable. Selecting internal RC for a
sketch does not remove the bootloader's oscillator requirement. PA18/PA19 are
OSC0 pads, so do not configure them as test outputs on such a board.

Factory parts start in ISP because ISP_FORCE is initially set. Erased
application flash alone is not a reliable entry method after configuration
changes. A populated application normally starts when the ISP condition is
inactive. DFU commands can set ISP_FORCE even during an identity query; a
successful `launch` clears the programming session and requests a watchdog
reset. A failed upload intentionally stays in DFU. Manual reset alone may
therefore not start an application after a probe. See [AVR32784, sections 6.2–6.3](https://www.microchip.com/content/dam/mchp/documents/OTH/ApplicationNotes/ApplicationNotes/doc7745.pdf).

Bootloaders 1.1.0 and later move ISP_FORCE/I/O configuration into two user-page
words. Their application offset and first-SRAM-word reservation remain the
same. Do not apply the old fuse configuration procedure to them. This uploader
accepts the documented legacy versions 1.0.0–1.0.3 or the 1.1.0 signature path;
other versions stop for review. [AVR32806, sections 6.1–6.3](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32166.pdf).

## Windows tool and USB driver

Use the official **dfu-programmer 1.1.0 x64** release:
[download/release](https://github.com/dfu-programmer/dfu-programmer/releases/tag/v1.1.0).
Extract the whole ZIP, retaining `libusb-1.0.dll` and licenses beside the EXE.
The tested `dfu-programmer-x64-1.1.0.zip` SHA-256 is
`0ab135fd9f7c54d142fdd9112f2183d82f5a362032dc3d52860a84756fe96e27`.
No binary is redistributed by UC3Bduino. This is a pinned tested version,
not a claim that it is the newest release.

```powershell
$env:UC3B_DFU_TOOL_PATH = 'D:\Tools\dfu-programmer-1.1.0' # actual extracted directory
& "$env:UC3B_DFU_TOOL_PATH\dfu-programmer.exe" --version
```

Factory B0/B1 uses **VID 03EB / PID 2FF6**. Check Device Manager's
Properties > Details > Hardware Ids. This is a DFU device, not a COM port.
Display name/category depend on the installed INF; no fixed category is
assumed. If libusb cannot open it, use [Zadig](https://zadig.akeo.ie/),
Options > List All Devices, select **only the device with 03EB:2FF6**, and
install/select **WinUSB**. Do not select a serial adapter or a hub. The
[upstream Windows driver notes](https://github.com/dfu-programmer/dfu-programmer/blob/v1.1.0/README.md#windows-driver-files)
also discuss libusb-win32/libusbK; this workflow uses WinUSB. Driver setup
on a physical board remains untested here.

The VID/PID and Atmel memory commands are documented by
[AVR32760](https://ww1.microchip.com/downloads/en/AppNotes/doc32131.pdf).
Generic `dfu-util` is not a substitute: this bootloader uses Atmel-specific
commands inside DFU requests. dfu-programmer's
[target table](https://github.com/dfu-programmer/dfu-programmer/blob/v1.1.0/src/arguments.c)
explicitly includes `at32uc3b0256`, 256 KiB total flash and an 8 KiB boot area.

## Identity and safety checks

Only one physical 03EB:2FF6 device may be connected. Windows PnP enumeration
also counts a matching device with an unusable driver. The wrapper then resolves
libusb's bus/address and pins every subsequent command to it, rechecking the
Windows device identity before erase, flash and launch. Do not unplug or swap
boards during an upload. There is no unique USB serial number in the factory
descriptor, and no automatic selection among multiple boards.

The old signature `58/20/02` identifies the UC3B processor family, **not its
flash density or package**. It is accepted only after the user has checked the
physical AT32UC3B0256 marking and explicitly supplied
`-LegacyDevice AT32UC3B0256` to the installer or uploader. This is not automatic
exact-part detection. See the [legacy Atmel signature definitions](https://github.com/MicrochipTech/avrfreaks-projects/blob/9e6bc971411a2d69374012572b3b6144f2f3f7f2/projects/uc3b-based-mp3-player/UC3B-MP3/workspace/UC3B-MP3Player/src/SOFTWARE_FRAMEWORK/UTILS/conf_isp.h)
and [legacy processor-ID extraction](https://github.com/schmidtw/open-roadster-crooner/blob/3e802c5b0418bd41cc3d2a64c88f93f5991ab4b9/apps/dfu/usb_dfu.c).

The newer signature path accepts `1F/1E/E4/revision` with bootloader byte
`0x10`. It corresponds to JTAG ID `0xr1EE403F` from
[32059L, table 27-27](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32059.pdf)
and the vendor [ISP signature extraction](https://github.com/avrxml/asf/blob/68cddb46ae5ebc24ef8287a8d4c61a6efa5e2848/common/services/isp/flip/uc3/isp.c).
Unknown identities, unreadable/security-locked devices and BOOTPROT other than
3 are rejected before erase. A custom bootloader is outside this contract.

## Manual upload, then Arduino integration

Build Blink using the installed core and an explicitly mapped D0; see
[ARDUINO_IDE](ARDUINO_IDE.md). The uploader takes **Intel HEX**, not ELF or BIN.
HEX preserves absolute load addresses, including initialized `.data` in flash.
dfu-programmer maps bit 31 away for the Atmel flash protocol; do not strip the
addresses yourself. [Upstream HEX reader](https://github.com/dfu-programmer/dfu-programmer/blob/v1.1.0/src/intel_hex.c).

```powershell
# Offline image/tool validation; no USB commands:
.\scripts\test-upload.ps1 -BuildPath build/arduino/Blink -CheckOnly
# Optional identity probe. May set ISP_FORCE, but never erase/program/start:
.\scripts\upload-dfu.ps1 -ProbeOnly -LegacyDevice AT32UC3B0256
# Actual upload of the already compiled Blink, after entering DFU:
.\scripts\test-upload.ps1 -BuildPath build/arduino/Blink -LegacyDevice AT32UC3B0256
# Same production path for any generated application HEX:
.\scripts\upload-dfu.ps1 -Firmware build/arduino/Blink/Blink.ino.hex -LegacyDevice AT32UC3B0256
```

`-ToolPath` accepts the executable or its directory and overrides
`UC3B_DFU_TOOL_PATH`, then PATH. `-LegacyDevice` must only be used after checking
the actual chip. It never relaxes address, tool-version or BOOTPROT checks.

The wrapper validates every HEX checksum/record, address and overlap **before
erase**, rejects user-page/boot/fuse/RAM data and checks the application entry.
It locks the validated file against concurrent replacement until completion.
After identity/protection checks it executes exactly:

```text
dfu-programmer.exe at32uc3b0256:<bus>,<address> erase
dfu-programmer.exe at32uc3b0256:<bus>,<address> flash <absolute-application.hex>
dfu-programmer.exe at32uc3b0256:<bus>,<address> launch
```

These are the underlying commands, **not a replacement for the wrapper's
preflight checks**. `erase` includes blank checking; `flash` performs readback
validation by default. No `--force`, suppression, user-page, fuse or security
write options are used. Any nonzero status stops subsequent commands; failed
readback cannot launch the application. `launch` uses the default watchdog reset,
never `--no-reset`. [Command implementation](https://github.com/dfu-programmer/dfu-programmer/blob/v1.1.0/src/commands.c).

Arduino calls the same installed scripts through `tools.uc3bdfu.upload.pattern`;
`upload.tool.default=uc3bdfu` supports uploading without a serial port.
`upload.protocol=atmel-dfu` selects normal Upload behavior. See the
[Arduino platform specification](https://docs.arduino.cc/arduino-cli/platform-specification/#sketch-upload-configuration).

```powershell
arduino-cli compile --fqbn uc3bduino:avr32:uc3b0256 --build-path build/arduino/Blink examples/Blink
arduino-cli upload --fqbn uc3bduino:avr32:uc3b0256 --input-dir build/arduino/Blink --verbose examples/Blink
```

Use the same `--config-file` for both commands if an isolated sketchbook was
installed. `--input-dir` above refers to the compile `--build-path`, containing
`Blink.ino.hex`; do not confuse it with renamed Export Compiled Binary files.
No `--port COM...` is required. In IDE 2.x: select the board, open Blink,
Verify, enter DFU, click **Upload**. The final step requests automatic application
start. The USB DFU device should disappear; this stage creates no replacement
COM port. Observe D0 with a verified LED circuit or oscilloscope.

## Troubleshooting

| Message/symptom | Action |
| --- | --- |
| Upload tool not installed / unsupported version | Extract the pinned full ZIP; set tool path and reinstall or restart IDE for environment changes |
| Firmware file not found / malformed HEX | Compile the sketch; pass its `.ino.hex` from the build directory |
| Unsafe firmware address | Rebuild with Stage 5 linker; do not suppress this check |
| DFU device not found | Check reset/PA13, OSC0, USB wiring/cable and Device Manager |
| Multiple UC3B DFU devices | Disconnect all but the intended board |
| Legacy identity requires confirmation | Read MCU marking; only then configure `-LegacyDevice AT32UC3B0256` |
| Unsupported identity / BOOTPROT | Stop and inspect actual chip/bootloader; no automatic fuse repair |
| Tool cannot open/read device | Close other flash tools; check WinUSB, security state and full ZIP contents |
| DFU erase/flash failed | Do not launch incomplete firmware; restore DFU and retry after resolving the reported cause |
| DFU launch failed | Programming may have finished, but start is unconfirmed; diagnose reset/boot configuration |
| No Blink after upload | Check actual D0 wiring and polarity, oscillator configuration and hardware execution separately |

## Next stage: USB CDC handoff

The bootloader uses the on-chip **USBB** peripheral. The current clock HAL
provides RC or undivided OSC0 only; **it does not generate the required 48 MHz
USB clock**. Stage 6 must configure a suitable PLL and `GCLK_USBB`, honor PLL
lock/VCO/divider constraints and USB's ±0.25% tolerance, enable peripheral clocks,
initialize endpoints/descriptors and explicitly manage USB detach/attach.
For example, 12 MHz can feed a 96 MHz VCO divided to 48 MHz; exact register
settings and board crystal tolerance still need validation. See
[32059L, PM and USBB sections](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32059.pdf).

The reset-based launch avoids inheriting live bootloader PLL/USB state. CDC must
enumerate as a new application device after the DFU disconnect, not reuse the
factory DFU identity or assume its endpoints are ready. Without reset, PM and
some USB fields may retain state; that handoff is deliberately unsupported here.
[AVR32806 section 6.3](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32166.pdf).
