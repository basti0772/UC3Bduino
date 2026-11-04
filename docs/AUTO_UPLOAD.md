# Stage 8.9 – Automatic USB upload

Status: repeated automatic hardware cycles passed on 2026-10-30 on the
AT32UC3B0256 revision I / 12 MHz reference board. Stage 9 is tracked in [SPI](SPI.md). The GP31
backend refuses old/unknown chip revisions because of FLASHC errata. An earlier
cold-reconnect descriptor error remains unresolved; see the evidence below.

## Reference and bootloader analysis

The available board is AT32UC3B0256 with a 12 MHz OSC0 crystal, PA23 active-low
red LED, and the original factory DFU 1.0.2 (reported version byte `0x02`).
The existing protected uploader observed legacy signature `58/20/02/03` and
`BOOTPROT=3`. The linker places the application at `0x80002000`; factory code
occupies the protected first 8 KiB (`0x80000000..0x80001FFF`). Application HEX
validation, BOOTPROT checks, unique USB target selection and readback remain
mandatory in `scripts/upload-dfu.ps1`.

Atmel AVR32784 (doc7745C), sections 6.3 and 7.2, describes the legacy 1.0.x
bootloader: an application watchdog reset normally starts the application again.
There is no documented timed DFU window on that path. The SRAM word `ISPK`
(`0x4953504B`) distinguishes an already-running ISP on some reset paths; it is
not a general application-to-DFU request. No invented SRAM magic is used.

The documented internal entry sets **nonvolatile general-purpose fuse 31,
ISP_FORCE**, then resets the MCU. This is a real limitation of the installed
bootloader. It conflicts with a strict requirement for a volatile-only marker:
after an interrupted upload or power failure, ISP_FORCE remains set and a power-on
reset enters DFU. The bootloader clears it when the existing uploader sends
`launch` after successful flash/readback. This persistent recovery behavior is
also used by the bootloader's own DFU commands.

Consequently the GP31 backend is **off by default**. Enabling it is an explicit
exception to the volatile-marker requirement, not an SRAM implementation. It
must not be enabled on an unverified bootloader (in particular factory 1.1.x
uses a different configuration). No bootloader replacement, flash page erase,
EEPROM write, user-page marker, BOOTPROT change or other fuse command is added.

Primary references:

- [AVR32784 / doc7745C, §§6.1, 6.3, 7.2](https://ww1.microchip.com/downloads/en/DeviceDoc/doc7745.pdf)
- [AT32UC3B datasheet 32059L](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32059.pdf),
  chapters 10, 11, 14 and 31: reset causes, watchdog, FLASHC and silicon errata.

## Firmware behavior

`UC3BDUINO_USB_AUTO_UPLOAD=1` enables the portable CDC touch state machine.
A valid seven-byte SET_LINE_CODING with baud 1200 arms it. Another baud disarms
it. Only a subsequent DTR falling edge requests reboot; repeated DTR=0 and baud
1200 alone do not. No UART configuration is changed.

The implementation waits for the close request's status ACK and 20 full-speed
USB frames. An unacknowledged/aborted close never resets. Reopening DTR, a baud
other than 1200, USB reset, reconfiguration or suspend cancels a pending reboot.
Repeated 1200-baud SET_LINE_CODING requests preserve an acknowledged countdown;
Windows sends such duplicates while changing DTR. The
request is consumed once even if the backend refuses it. A following SETUP that
hides the prior status completion conservatively cancels the unconfirmed close;
the host reports a timeout and can retry.

`UC3BDUINO_FACTORY_DFU_GP31=1` selects the separate, opt-in legacy backend.
It verifies BOOTPROT=3, issues only FLASHC EGPB for bit 31 if needed, checks
bounded readiness and command errors, then verifies all other fuse bits stayed
unchanged. If preparation fails, USB remains active and no reset is attempted.
Before reading that fuse register or issuing a command, it also checks the OCD
device ID and accepts only AT32UC3B0256 revisions I/J/K. The reference board was
identified as revision I (`DID=0x81EE403F`) by a read-only diagnostic on
2026-10-30. The legacy bootloader's reported product revision is not used as a
substitute for the MCU's OCD ID. Only revision I has been physically tested.

Datasheet 32059L §31.2 documents a different FGPFRLO address for revision B and
a flash-fetch corruption erratum after fuse commands for B/F/G. Those revisions
require a RAM routine and a dummy page-write workaround; the simple backend
does not implement that workaround and must refuse them. No dummy page write
has been added or issued by this stage.

After successful preparation, USB detaches and stops. With interrupts masked,
the CPU COUNT register provides a 250 ms disconnect interval. Keyed watchdog
writes enable PSEL=15 (about 569 ms at nominal 115.2 kHz RCSYS), then the CPU
waits for reset. This produces **RCAUSE.WDT**, resets the CPU and peripheral
domains, and starts the normal boot vector. There is no direct jump into factory
code. The watchdog itself survives its own reset; the documented ISP entry path
disables it. No persistent reset loop is intentionally configured.

## Setup and commands

Use a fresh sketchbook; the installer never overwrites an existing platform.
Supply locally authorized USB IDs, the external AVR32 compiler and the existing
dfu-programmer 1.1.0 path. The additional switch acknowledges the GP31 behavior
above and must only be used after confirming factory DFU 1.0.2:

```powershell
.\scripts\install.ps1 -SketchbookPath C:\UC3B-auto `
  -ToolchainPath $env:AVR32_TOOLCHAIN_PATH -DfuToolPath $env:UC3B_DFU_TOOL_PATH `
  -UsbVendorId $usbVendorId -UsbProductId $usbProductId `
  -LegacyDevice AT32UC3B0256 -EnableLegacyAutoUpload
```

The initially running sketch must already include this enabled core and call
`SerialUSB.begin()`. Bootstrap once using manual factory DFU:

```powershell
.\tools\Test-AutoUpload.ps1 -SketchbookPath C:\UC3B-auto `
  -OutputDirectory C:\UC3B-build-bootstrap -BuildOnly
# Enter factory DFU, then use its protected existing uploader:
.\scripts\upload-dfu.ps1 `
  -Firmware C:\UC3B-build-bootstrap\AutoFlashTest\AutoFlashTest.ino.hex `
  -ToolPath $env:UC3B_DFU_TOOL_PATH -LegacyDevice AT32UC3B0256
```

For subsequent application HEX files, close Serial Monitor and use the current
CDC port (COM7 is an example, not a stored default):

```powershell
.\tools\AutoUpload.ps1 -Port COM7 -Firmware C:\build\Application.ino.hex `
  -ToolPath $env:UC3B_DFU_TOOL_PATH -LegacyDevice AT32UC3B0256
```

Omit `-Port` and supply both `-UsbVendorId` and `-UsbProductId` for discovery;
exactly one matching USB COM interface is required. Non-USB COM ports are never
selected. The source CDC USB location must match the emerging `03EB:2FF6` DFU
device and the returning CDC interface; the COM number may change. Keep the
board on the same hub/connector. Pre-existing or multiple DFU devices abort the
automatic path. Already-in-DFU recovery uses `scripts/upload-dfu.ps1` directly.

The Windows host first opens at 115200, asserts DTR, then sets baud 1200 and
lowers DTR after 150 ms. This establishes the intended edge before arming;
[.NET initializes DTR disabled during port opening](https://github.com/dotnet/runtime/blob/main/src/libraries/System.IO.Ports/src/System/IO/Ports/SerialStream.Windows.cs).
The baud is only USB metadata and never reconfigures a physical UART.

HEX bounds and programmer version are checked before touching CDC. The same
file stays locked against write/delete until completion. The existing uploader
retains signature/BOOTPROT checks, a process-wide mutex, pinned bus/address,
mandatory readback and abort-on-error. The automatic path also requires the
observed bootloader version byte `0x02` before erasing. Native flasher failures
retain their exit code; other failures return 1. No failed phase prints a pass
for a later phase. Detection defaults are 20 seconds per reboot phase and 30
seconds for the returning application; `-DfuTimeoutSeconds` and
`-ApplicationTimeoutSeconds` accept 1..120 seconds. Existing native programmer
calls each have a 60-second limit. OS USB/serial calls must also return normally.

Build, touch, upload and verify a new unique identity in one command:

```powershell
.\tools\Test-AutoUpload.ps1 -SketchbookPath C:\UC3B-auto -Port COM7 `
  -LegacyDevice AT32UC3B0256 -OutputDirectory C:\UC3B-cycle-001
```

Use a new output directory for each run. The test saves the exact generated
sketch, GUID (`build-id.txt`), compiler log, ELF/HEX/BIN/map and received
`cdc.log`. The ID must match the current build, and the same response must name
AT32UC3B0256, 12 MHz and USB CDC. Reusing output from an old application cannot
pass. `-BuildOnly` creates the bootstrap without touching hardware. LED flashing
still needs a visual observation. A complete cycle does not measure clock
accuracy or prove UART/ADC operation. The sketch toggles PA23 active low and
does not depend on a terminal being open for blinking.

## Arduino integration

Only the opt-in installation overrides `tools.uc3bdfu.upload.pattern` in its
generated `platform.local.txt`, passing Arduino's selected `{serial.port}` to
the installed `tools/AutoUpload.ps1`. Arduino's own 1200-baud touch and upload
port wait are explicitly disabled to avoid a double touch; the script handles
the genuine USB DFU device rather than looking for a bootloader COM port.
The default platform keeps the original manual DFU upload recipe.

After bootstrap, an enabled installation supports these interfaces. CLI upload
passed on the reference board; IDE GUI acceptance is still untested:

```powershell
arduino-cli compile --fqbn uc3bduino:avr32:uc3b0256 --build-path C:\build C:\sketch\AutoFlashTest
arduino-cli upload --fqbn uc3bduino:avr32:uc3b0256 --port COM7 --input-dir C:\build C:\sketch\AutoFlashTest
```

Use the same CLI configuration/sketchbook for both commands. Future sketches
must call `SerialUSB.begin()` and retain the enabled backend if the next upload
should also be automatic. Uploading Blink or an image without USB is allowed,
but CDC verification will time out and the next upload requires manual DFU.

## Verification and acceptance

On 2026-10-29 the existing reference board enumerated as COM5 and transmitted
repeated USBSerialHello greetings and millisecond values. That is baseline USB
evidence, **not** an automatic reboot test. The owner explicitly authorized the
GP31 exception for this board. The enabled bootstrap firmware built successfully
with GCC 4.3.2 and passed ELF/HEX/BIN/SRAM validation; its bootstrap upload and
CDC identification subsequently passed.

- 23 native test executables passed, including enabled/disabled touch and legacy
  backend, FIFO, EP0, CDC, UART, timing and ADC mocks.
- 56 synthetic automatic-upload checks passed, including Windows touch ordering, discovery ambiguity,
  physical location mismatch, timeouts, native error propagation, image locking
  and rejection of stale/wrong build, board and clock identity.
- 51 protected DFU checks passed, including preservation of native error codes
  and rejection of a changed target before erase.
- All 15 Arduino examples built from a fresh default-backend installation and
  passed artifact validation. The enabled AutoFlashTest built separately.
- All 38 installer/archive checks passed, including the optional Arduino recipe,
  unchanged manual default, source hashes and installation from an extracted ZIP.
- Disassembly confirms the generated sole FLASHC command is `0xA5001F08`
  (EGPB, bit 31), and the watchdog receives `0x55000F01`, `0xAA000F01`, then a
  counter clear. This verifies generated instructions, not real reset behavior.
- The manual bootstrap passed: build `b4eb43fc-eac9-428c-b0a2-d8948bb98a2f`
  was flashed with readback and identified over COM5. The owner confirmed the
  red LED blink on 2026-10-30.
- On 2026-10-30 the first automatic attempt failed while opening CDC, before
  erase/flash. A normal USB reconnect produced a descriptor error. Launching
  the stored application from factory DFU restored CDC, and two normal
  open/read/close cycles passed. The revised touch sequence was sent, but CDC
  did not disappear within 20 seconds; no erase followed that timeout.
- The read-only diagnostic identified revision I, `FGPFRLO=0x7C07FFFF`, and
  `RCAUSE=0x08` after factory DFU launch. Both immediate and delayed port-close
  tests received DTR falling edges and status ACKs, but four repeated 1200-baud
  settings per touch canceled the old countdown. The corrected state machine
  preserves identical 1200-baud settings; its regression test and target build
  pass. The corrected production bootstrap `193c58ab-c736-448a-8b33-ba7bb898d061`
  passed protected upload/readback and CDC identification before the cycles below.
- Two consecutive automatic cycles then passed without manual DFU or USB
  intervention: explicit COM5 selection, followed by discovery using locally
  configured VID/PID. Each run built a new GUID, observed CDC disappearance and
  factory DFU at the same physical USB location, checked signature/BOOTPROT,
  flashed with mandatory readback, launched, rediscovered COM5 and read the new
  identity. The previous identity could not satisfy either test.
- A third new build passed `arduino-cli upload` through the installed opt-in
  recipe, including automatic DFU entry, protected flash/readback and CDC return.
  The new identity was independently read after the CLI returned successfully.
- Arduino IDE GUI, UART and ADC hardware acceptance have not been performed.
  No cold-start/reconnect pass is claimed; the earlier descriptor failure remains
  a separate unresolved observation. Other MCU/clock/revision combinations are
  not hardware-validated. Interrupted-upload recovery was not fault-injected.

### Reproduction and local evidence

The following commands were run from the repository on the reference Windows
host. USB IDs remain local configuration, not public core defaults. The scratch
evidence is deliberately not shipped in the source archive.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-AutoUpload.ps1 `
  -ConfigFile .scratch/stage089/fixed-cli.yaml -Port COM5 `
  -LegacyDevice AT32UC3B0256 -OutputDirectory .scratch/stage089/cycle-002
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-AutoUpload.ps1 `
  -ConfigFile .scratch/stage089/fixed-cli.yaml -UsbVendorId 9025 -UsbProductId 62 `
  -LegacyDevice AT32UC3B0256 -OutputDirectory .scratch/stage089/cycle-003
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-AutoUpload.ps1 `
  -ConfigFile .scratch/stage089/fixed-cli.yaml -LegacyDevice AT32UC3B0256 `
  -OutputDirectory .scratch/stage089/cli-cycle-004 -BuildOnly
& 'C:\Program Files\Arduino CLI\arduino-cli.exe' `
  --config-file .scratch/stage089/fixed-cli.yaml upload `
  --fqbn uc3bduino:avr32:uc3b0256 --port COM5 `
  --input-dir .scratch/stage089/cli-cycle-004/AutoFlashTest --verbose `
  .scratch/stage089/cli-cycle-004/source/AutoFlashTest
```

| Run | New build identity | Result / local evidence |
| --- | --- | --- |
| Explicit COM | `45b1daf6-33c5-4a96-b9d8-dc5cf4bd0358` | PASS: `.scratch/stage089/cycle-002.log`, `cycle-002/cdc.log` |
| USB ID discovery | `af29db35-873b-4bd1-825f-176cb98fc90c` | PASS: `.scratch/stage089/cycle-003.log`, `cycle-003/cdc.log` |
| Arduino CLI recipe | `f68f4223-e38c-4250-9cb8-077b40f1186b` | PASS: `.scratch/stage089/cli-cycle-004-upload.log`, `cli-cycle-004/cdc.log` |

### Changed files (Stage 8.9)

- Firmware: `src/usb/usb_boot_touch.{c,h}`, `usb_cdc.c`, `usb_config.h`,
  `usb_device_core.c`, `uc3b_usb_device.{c,h}`; `src/hal/uc3b_bootloader.{c,h}`
  and `uc3b_bootloader_hw.h`.
- Host: `tools/AutoUpload.ps1`, `Test-AutoUpload.ps1`, `auto-upload-common.ps1`;
  `scripts/dfu-common.ps1`, `upload-dfu.ps1`, `install.ps1`,
  `install-arduino-hardware.ps1`.
- Example: `examples/AutoFlashTest/AutoFlashTest.ino`.
- Tests: `tests/usb_boot_touch_test.c`, `bootloader_test.c`,
  `usb_protocol_test.c`, `run-usb-host-tests.cmd`, `test_auto_upload.ps1`,
  `test_dfu_upload.ps1`, `test_packaging.ps1`; `.github/workflows/build.yml`.
- Documentation and archive manifest: `README.md`, `CHANGELOG.md`,
  `docs/USB_CDC.md`, `docs/AUTO_UPLOAD.md`, `packaging/source-files.txt`.

The preceding working Stage 8.5 state is preserved in local commit `41b5633`;
Stage 8.9 is isolated on `codex/stage-08-9-auto-upload`. No bootloader replacement,
Stage 9 implementation, release, remote push or PR merge is part of this step.

## Recovery and scope

If a touch times out, close any serial monitor and enter factory DFU manually.
If flashing fails after entry, leave the board in DFU and retry the protected
uploader with a known-good application. A USB disconnect/reconnect with the
board's normal DFU procedure remains the recovery path. Never remove BOOTPROT
or replace the bootloader to make an automatic test pass.

Only the AT32UC3B0256 / 12 MHz board is eligible for the experimental backend.
The portable touch module and host orchestration do not select MCU registers;
future MCU/clock profiles need separate validation. UART and ADC are outside
this stage's hardware acceptance.
