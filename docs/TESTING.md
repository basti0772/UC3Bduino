# Test record

## Stage 10 — Wire / software I2C master

Executed **2026-10-31** on Windows with AVR32 GCC/G++ 4.3.2, Arduino CLI
1.5.2-rc.1, MSVC and the AT32UC3B0256 revision I / 12 MHz reference board.
Baseline Stage 9 is preserved in `fe9572b`; work is on `codex/stage-10-wire`.
This implementation uses open-drain GPIO, **not the native TWI peripheral**.

| Check | Result | Evidence / limits |
| --- | --- | --- |
| Native regression | PASS | 27 executables, including the actual software I2C bit engine and Wire API with mock adapters; `native-final.log` |
| All Arduino examples | COMPILE PASS | 20 sketches from a fresh default-backend installation, ELF/HEX/BIN/SRAM validation; `regression.log`, `final-regression-build/` |
| Auto-upload-enabled Wire examples | COMPILE PASS | All three Wire sketches from a separate fresh enabled installation; `build-final.log`, `build-final/` |
| Bare-metal regression | COMPILE PASS | minimal, startup-check and gpio-toggle with artifact validation; `bare-regression.log` |
| Packaging / installation | PASS | 42 checks including Wire source/example inclusion, reproducible archives and extracted installation; `packaging.log` |
| Protected DFU / automatic host lifecycle | PASS | 51 DFU and 56 automatic-upload synthetic checks; `dfu-host.log`, `auto-host.log` |
| Metadata / syntax / whitespace | PASS | 170 source manifest entries, PowerShell parser and git diff check |
| Wire API smoke | HARDWARE PASS | Build `66ca6081-cc20-45d8-aebe-542ab671a505`: 43 cases, 0 failures; exact CDC identity in `hardware.log`, `hardware-cdc.log` |
| Automatic upload into Wire smoke | HARDWARE PASS | CDC touch, protected DFU upload/readback and CDC return; `hardware.log` |
| Direct automatic upload after Wire smoke | FAILED BEFORE TOUCH | COM5 open failed before reset/erase; `auto-regression.log` |
| Factory-DFU recovery | HARDWARE PASS | Protected upload/readback and exact AutoFlashTest identity `529f16f3-9a30-4feb-8456-1deb92e8bc60` on COM6; `recovery.log`, `auto-regression/recovery-cdc.log` |
| Automatic cycle after recovery | HARDWARE PASS | 1200-baud touch, DFU, protected upload/readback and exact new CDC identity `877a1dbb-543e-4b96-bd61-411d91d140da` on COM6; `auto-regression-retry.log`, `auto-regression-retry/cdc.log` |
| External I2C bus / device | HARDWARE NOT TESTED | Owner confirmed no I2C wiring or pull-ups; no physical ACK, repeated START, bus rate, stretching or electrical timing claim |
| Other MCU/clock variants | HARDWARE NOT TESTED | Only reference 0256 revision I / 12 MHz board exercised |

Evidence paths above are relative to `.scratch/stage10`; local USB IDs and logs
are excluded from the source archive. The runtime smoke initializes/releases
PA09/PA10 and tests API/buffer/error behavior without sending START or accessing
an I2C device. Final builds additionally include compile-time GPIO layout checks
and integer `write()` overloads, covered by the final native suite; these additions
were not separately flashed in the smoke run.

The initial automatic regression encountered the previously observed Windows CDC
open failure despite COM5 being listed and stopped before reset/erase. After the
owner activated Factory-DFU, the prepared AutoFlashTest was loaded successfully.
The application appeared as COM6 on a different USB topology. A fresh AutoFlashTest
then completed the full automatic cycle without another manual DFU action.
The final board runs build `877a1dbb-543e-4b96-bd61-411d91d140da` on COM6.
Recovery and retry both verified BOOTPROT=3, protected the first 8 KiB and required
flash readback. The factory bootloader was not replaced.

This successful retry does not establish an I2C cause or resolve the intermittent
CDC-open/USB cold-reconnect issue, especially across USB topologies. No cold-start
or prolonged port-reopening test was performed. Native TWI offload and external
I2C acceptance remain separate future work; see [WIRE](WIRE.md) for the supported
subset, timing bounds and required wiring.

## Stage 9 — SPI

Executed **2026-10-30** on Windows with AVR32 GCC/G++ 4.3.2, Arduino CLI
1.5.2-rc.1, MSVC and the AT32UC3B0256 revision I / 12 MHz reference board.
Baseline Stage 8.9 is preserved in `41ca7e2`; work is on `codex/stage-09-spi`.

| Check | Result | Evidence / limits |
| --- | --- | --- |
| Native regression | PASS | 25 executables: existing 23 plus real SPI HAL/API tests with mock adapters; `.scratch/stage09/native-final.log` |
| All Arduino examples | COMPILE PASS | 17 sketches from a fresh default-backend installation, ELF/HEX/BIN/SRAM validation; `regression.log` and `final-regression-build/` |
| Auto-upload-enabled SPI examples | COMPILE PASS | Both SPI sketches from a separate fresh enabled install; `build-final.log`, `build-final/` |
| Bare-metal regression | COMPILE PASS | minimal, startup-check and gpio-toggle; artifact validation; `bare-regression.log` |
| Packaging / installation | PASS | 40 checks including SPI core/example inclusion, reproducible archives, hashes, extracted install; `packaging.log` |
| Protected DFU / automatic host lifecycle | PASS | 51 DFU and 56 auto-upload synthetic checks; `dfu-host.log`, `auto-host.log` |
| Metadata / syntax / whitespace | PASS | 156 source manifest entries, PowerShell parser, git diff check |
| Internal SPI loopback | HARDWARE PASS | Final build `03188aa4-236b-4359-9069-6821b626ef75`: 25 cases, 0 failures; `hardware-final.log` and `hardware-final-cdc.log` |
| Automatic upload after SPI | HARDWARE PASS | From final loopback firmware to new AutoFlashTest build `1b449fd8-275b-4c94-9e4b-957b7116ac7e`, protected readback and exact CDC identity; `auto-regression.log`, `auto-regression/cdc.log` |
| External SPI bus / slave | HARDWARE NOT TESTED | Owner reported no connected SPI device/jumper; no signal timing, external bit-order, voltage or device protocol claim |
| Other MCU/clock variants | HARDWARE NOT TESTED | Only reference 0256 revision I / 12 MHz board exercised |
| UART / ADC hardware, IDE GUI | HARDWARE NOT TESTED | No new claims; earlier USB cold-reconnect descriptor failure remains unresolved |

Paths in the evidence column are relative to `.scratch/stage09`; private local
USB identity and logs are not included in the archive. The final board runs the
new AutoFlashTest on COM5 and remains available for automatic upload. The
bootloader was not replaced and every upload checked BOOTPROT=3 and protected
application boundaries before erasing/programming.

The SPI suite runs as part of `tests/run-host-tests.cmd` in an MSVC Native Tools
prompt, including CI. `tests/test_packaging.ps1` checks the installer/archive.
`tests/verify_firmware.py <build-directory> --basename <sketch>.ino` checks each
real Arduino artifact. The final hardware commands reused the existing uploader:

```powershell
. ./scripts/dfu-common.ps1
. ./tools/auto-upload-common.ps1
$application = Invoke-AutoUpload `
  -Firmware .scratch/stage09/build-final/SPILoopback/SPILoopback.ino.hex `
  -Port COM5 -LegacyDevice AT32UC3B0256 `
  -ToolPath .scratch/stage05/dfu-programmer-1.1.0/dfu-programmer.exe
# The local hardware-final.ps1 harness then reads CDC at 115200 with DTR high,
# requiring the exact new GUID followed by Cases: 25, Failures: 0, RESULT: PASS.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-AutoUpload.ps1 `
  -ConfigFile .scratch/stage09/cli.yaml -Port COM5 `
  -LegacyDevice AT32UC3B0256 -OutputDirectory .scratch/stage09/auto-regression
```

See [SPI](SPI.md) for pin mapping, API behavior, error recovery and acceptance
limits. These results do not close the separate Stage 8.5 public-release gate.

## Stage 8 – ADC / analogRead

Executed **2026-10-28**, Windows 11, PowerShell 5.1, AVR32 GCC/G++ 4.3.2,
binutils 2.19.20081016, Make 3.81, Arduino CLI 1.5.2-rc.1 and MSVC native tests.
Platform **0.8.0-dev**, FQBN **uc3bduino:avr32:uc3b0256**. No MCU hardware or
Arduino IDE GUI was available and no firmware was uploaded.
**COMPILE PASS; STATIC REVIEW PASS; HARDWARE NOT TESTED.**

| Check | Status | Evidence / limits |
| --- | --- | --- |
| ADC initialization | HOST PASS / COMPILE PASS | Lazy init once, PBA-derived prescaler, startup wait, PM-ready failure and reinitialization after timeout; actual cell startup unmeasured |
| ADC clock/timing | HOST PASS / STATIC REVIEW PASS | RCSYS, 0.4/8/12/16/60 MHz PBA and divided-bus fixture; <=2.5 MHz target, 128 startup and 18 tracking cycles; clock formulas and fields checked against 32059L |
| analogRead(A0) compile | COMPILE PASS | Installed AnalogRead with USB output and a separate ADC_USE_USB=0 UART build |
| A0–A7 and invalid pins | HOST PASS / COMPILE PASS | Real variant/wrapper, all eight pad/channel/mux combinations, invalid index and D0 unconfigured/PA03 alias/PB03 non-ADC cases; no invalid-pin GPIO/ADC access |
| ADC channel switching | HOST PASS / COMPILE PASS | Actual HAL state machine, channels 0..7, one channel at each START, old EOC drained, result consumed before CHDR; AnalogMultiChannel builds |
| ADC failure behavior | HOST PASS | CPU COUNT wrap, stopped COUNT, conversion and clock timeouts, unchanged output on HAL failure, reset before channel changes, retry, nested HAL busy guard and IRQ mask preservation |
| GPIO regression | HOST PASS / COMPILE PASS | Digital wrapper uses the extended map; all GPIO sketches compile; integration fixture uses D0=PB00 separate from ADC pads |
| Timing / UART / USB regression | HOST PASS / COMPILE PASS | Existing native suites pass; ArduinoMinimal, Blink, SerialHello, USBSerialBlink and every other existing sketch compile |
| USB + ADC | COMPILE PASS | Both examples with SerialUSB; combined ADC/USB/UART/GPIO/millis/micros/delay/delayMicroseconds fixture at RC, 8 and 16 MHz |
| UART + ADC | COMPILE PASS | Both examples with HardwareSerial at 115200, U0C/12 MHz fixture; shared Print unchanged |
| analogRead(A0) hardware | HARDWARE NOT TESTED | No physical conversion, timing or reference measurement |
| ADC minimum input | HARDWARE NOT TESTED | Ground input acceptance pending |
| ADC mid-scale input | HARDWARE NOT TESTED | Measured half-reference source acceptance pending |
| ADC maximum input | HARDWARE NOT TESTED | Input at verified ADVREF acceptance pending |
| Channel switching hardware | HARDWARE NOT TESTED | Alternating low/high levels, source impedance and first-read settling pending |
| USB + ADC hardware | HARDWARE NOT TESTED | No USB enumeration, data delivery, IRQ-latency or supply-noise measurement |
| UART + ADC hardware | HARDWARE NOT TESTED | No physical serial output or concurrent timing measurement |

All **fourteen installed examples** build with `-Wall -Wextra -Werror`. The
12 MHz crystal/U0C/PA03 setup is a compilation fixture, not a board assertion.
USB IDs remain zero; all USB paths link but the firmware cannot attach. The RC
integration build intentionally cannot run USB or 115200-baud UART. ELF/HEX/BIN/
map validation passes for the fourteen examples, both UART alternatives, the
three integration builds and all three bare-metal programs. Bootloader/SRAM/
stack reservations remain intact; no unresolved runtime globals or constructor
sections. DFU CheckOnly accepts all fourteen standard examples without USB I/O.
CLI library discovery finds both new examples; diagnosis and smoke tests pass.

`tests/run-host-tests.cmd` now runs **18 native executables** (four added for ADC
logic and variant/API configurations), all under `/W4 /WX`. Existing DFU tests
pass **42 synthetic-device checks**; packaging passes **26 host checks**, including
copied-source hashes, deterministic archives and extracted-source installation.
These results do not simulate electrical ADC behavior. CI runs native/package
checks only; it has no AVR32 compiler. Metadata, source allowlist, UTF-8 files,
PowerShell syntax and whitespace checks also pass.

Measured `avr32-size` text/data/bss bytes at the 12 MHz fixture (bss includes
the reserved 1 KiB stack):

| Example | text | data | bss |
| --- | ---: | ---: | ---: |
| ArduinoMinimal | 996 | 0 | 1036 |
| DigitalOutput | 1668 | 0 | 1036 |
| Blink | 1880 | 0 | 1036 |
| MillisTest | 1896 | 0 | 1044 |
| MicrosPulse | 1916 | 0 | 1040 |
| SerialHello | 2280 | 0 | 1120 |
| SerialMillis | 2592 | 0 | 1120 |
| SerialEcho | 2328 | 0 | 1120 |
| GpioSmoke | 1876 | 0 | 1036 |
| USBSerialHello | 6812 | 0 | 1416 |
| USBSerialEcho | 6572 | 0 | 1416 |
| USBSerialBlink | 7448 | 0 | 1416 |
| AnalogRead (USB) | 7432 | 0 | 1420 |
| AnalogMultiChannel (USB) | 7456 | 0 | 1420 |
| AnalogRead (UART) | 3300 | 0 | 1120 |
| AnalogMultiChannel (UART) | 3324 | 0 | 1120 |
| AnalogRead with constant 500 instead of ADC (USB control) | 6656 | 0 | 1416 |

Thus the AnalogRead example adds approximately **776 text bytes and 4 aligned
BSS bytes** relative to the otherwise identical constant-value USB control,
including ADC/error handling and the pin map. The ADC itself has two one-byte
state flags. The map grows from 2 to 36 read-only bytes; GPIO images grow by
48 text bytes including index/address code and alignment. Minimal and UART/USB
images that do not use the map retain Stage 7 sizes. Bare-metal minimal,
startup-check and gpio-toggle remain 680/0/1024, 740/8/1036 and 1088/0/1044.
No floating point, heap or vendor driver library is linked. Map/BIN span remains
the authority for flash occupancy because section totals can omit gaps.

Run `tests/run-host-tests.cmd` in a Visual Studio native-tools prompt for host
checks. Reinstall the canonical sources into a fresh sketchbook before target
builds, then compile the examples using the documented CLI commands. To reproduce
the UART alternatives set `ADC_USE_USB` to 0 in copies of the two examples;
for the resource control replace only `analogRead(A0)` with `500` in AnalogRead.
See [ADC](ADC.md) and [HARDWARE_TEST](HARDWARE_TEST.md) for the pending physical
test procedure and electrical limits. No release or tag is published.

## Stage 7 – Packaging / easy installation

Executed **2026-10-26/27**, Windows 11, PowerShell 5.1, AVR32 GCC/G++ 4.3.2,
binutils 2.19.20081016, Make 3.81, Arduino CLI 1.5.2-rc.1 and MSVC native tests.
FQBN remains **uc3bduino:avr32:uc3b0256**, platform version **0.7.0-dev**.
No MCU/USB hardware or Arduino IDE GUI was available. No firmware was uploaded.

- All twelve **installed** Arduino examples compile with warnings as errors.
  `arduino-cli lib examples UC3BduinoExamples --fqbn uc3bduino:avr32:uc3b0256`
  discovers all twelve bundled examples. CLI board discovery reports the stable FQBN.
- Real install used a CLI configuration with a sketchbook path containing spaces;
  no `-SketchbookPath` override was required. PA03/U0C/12 MHz crystal are compile
  fixtures only. USB IDs remain zero (never attach); complete USB code still links.
- Independent ELF/HEX/BIN/map checks pass on all twelve examples and all three
  bare-metal builds. Application starts at 0x80002000, excludes the bootloader,
  reserves the first SRAM word and 1 KiB stack, and has no unresolved globals.
  DFU `-CheckOnly` accepts all twelve images without sending USB commands.
- `diagnose.ps1` passes CLI/platform/compiler/DFU version/board and ArduinoMinimal
  compile checks. IDE is reported not found and DFU device not connected.
  `smoke-test.ps1` separately compiles installed Blink and USBSerialBlink, retaining
  logs/artifacts. Both commands exit successfully without any upload.
- `tests/run-host-tests.cmd` passes all fourteen native UART, Print, timing and
  USB protocol/API/clock executables. Existing DFU tests pass **42 host checks**
  with a synthetic device; their simulated erase/flash messages are not hardware activity.
- Packaging host tests cover explicit/environment/PATH resolution, paths with
  spaces, wrong targets/C++ versions/DFU versions, CLI configuration resolution,
  WhatIf, refusal to overwrite existing user data, pin conflicts, all copied-source
  hashes, rejected download checksums, deterministic ZIPs, archive contents/checksums
  and installation from extracted source without Git. They use an explicitly fake
  compiler and make no firmware-build claim.
- The optional DFU helper accepts the original 423421-byte upstream ZIP with the
  documented SHA-256 and extracts all DLLs/licenses; a bad archive is rejected
  before extraction. This checks offline extraction, not a fresh live download.
- UTF-8 sources, synchronized version metadata, complete source allowlist,
  PowerShell syntax and `git diff --check` pass. Firmware/HAL/startup/linker sources,
  board identity and compile/upload recipes are unchanged; only the Arduino header
  includes the new version macros. No third-party binaries enter the source archive.

Measured text/data/bss bytes (bss includes the reserved stack):

| Example | text | data | bss |
| --- | ---: | ---: | ---: |
| ArduinoMinimal | 996 | 0 | 1036 |
| DigitalOutput | 1620 | 0 | 1036 |
| Blink | 1832 | 0 | 1036 |
| MillisTest | 1848 | 0 | 1044 |
| MicrosPulse | 1868 | 0 | 1040 |
| SerialHello | 2280 | 0 | 1120 |
| SerialMillis | 2592 | 0 | 1120 |
| SerialEcho | 2328 | 0 | 1120 |
| GpioSmoke | 1828 | 0 | 1036 |
| USBSerialHello | 6812 | 0 | 1416 |
| USBSerialEcho | 6572 | 0 | 1416 |
| USBSerialBlink | 7400 | 0 | 1416 |
| Bare-metal minimal | 680 | 0 | 1024 |
| Bare-metal startup-check | 740 | 8 | 1036 |
| Bare-metal gpio-toggle | 1088 | 0 | 1044 |

The UART-only images are four bytes larger than the Stage 6 record. Symbol and
disassembly comparison isolates a wider conditional branch and alignment in
`uc3b_uart_init`; the canonical UART source/configuration is identical. This is
not a new hardware feature. GPIO and USB resource totals remain unchanged.

CI runs source/package and host tests only, with **no AVR32 compiler**. Local
firmware results above are distinct from CI and physical hardware validation.
The unpublished development source archive is prepared with checksums; no release,
tag or Boards Manager index is published. Follow [HARDWARE_TEST](HARDWARE_TEST.md)
for the still-pending DFU/launch/CDC/GUI acceptance run.

## Stage 6 – USB CDC / SerialUSB

Executed **2026-10-26**, Windows, AVR32 GCC/G++ 4.3.2, binutils 2.19.20081016,
Arduino CLI 1.5.2-rc.1, PowerShell 5.1 and MSVC 14.51.36231. Target builds use
`-Wall -Wextra -Werror`; native tests `/W4 /WX`. No UC3B board or USB bus analyzer
was available. **COMPILE PASS; STATIC REVIEW PASS; HARDWARE NOT TESTED.**

Target images retain VID/PID zero and cannot attach. The descriptor identity is
read at runtime, so the complete USB path is still linked/checked in these builds.
Only the native fake transport uses synthetic IDs. Compilation fixture: 12 MHz
OSC0 crystal, D0=PA03, Serial=U0C. These are not claims about the user's board.

| Check | Status | Evidence / limits |
| --- | --- | --- |
| USB clock configuration | COMPILE PASS / HOST PASS | 8/12/16 MHz PLL1→48 MHz; native register fixture checks lock timeout, missing oscillator, foreign ownership and unchanged CPU/bus clock selectors; RC/10 MHz rejected for USB |
| USB device build | COMPILE PASS | Full driver/core/CDC/SerialUSB compiled and linked with installed device-header ABI assertions |
| Twelve Arduino examples | COMPILE PASS | ArduinoMinimal, DigitalOutput, Blink, MillisTest, MicrosPulse, SerialHello, SerialMillis, SerialEcho, GpioSmoke and USBSerialHello/Echo/Blink |
| USB/UART/timing/GPIO combined | COMPILE PASS | Integration fixture links both Serial objects, flush/end/rebegin, GPIO, micros/millis and both delay APIs at 8 MHz, 16 MHz and RC; RC intentionally cannot run USB or 115200 UART |
| Bare-metal regression | COMPILE PASS | minimal, gpio-toggle, startup-check; existing sizes unchanged |
| Firmware layout | PASS | All ELF/HEX/BIN images independently checked; entry 0x80002000, bootloader excluded, SRAM word zero reserved, 1 KiB stack, no undefined runtime symbols |
| DFU image preflight | PASS | Production CheckOnly accepts all three USB HEX files; no USB commands or physical writes sent |
| Device descriptor | HOST PASS / STATIC REVIEW PASS | Bytes/length/class, short requests and missing serial tested; no physical GET_DESCRIPTOR observed |
| Configuration descriptor | HOST PASS / STATIC REVIEW PASS | 67 bytes, two interfaces, 64+3 packet split, endpoint addresses and packet sizes |
| EP0 control | HOST PASS | Read/write/status, no-data IN status, exact/full packet ZLP, early OUT, new SETUP abort, deferred address, configuration failures, stalls and halt requests |
| CDC class requests | HOST PASS | Line coding round trip/validation, coalesced status/new SETUP, DTR/RTS, SERIAL_STATE changes, unsupported requests stall |
| RX/TX buffers and flush | HOST PASS | Wrap/full, whole-packet backpressure predicate/re-enable, batched TX, ACK wait, final ZLP, short writes, timeout, RX preservation |
| SerialUSB API | HOST PASS | begin overloads, reused Print/CRLF/decimal conversion, binary writes, short counts, read/peek/flush/end |
| USB reset/suspend/reconnect logic | HOST PASS | Portable state/queues reset, suspension preserves state, end/begin and reconfiguration; no electrical/MMIO simulation |
| ISR/vector | STATIC REVIEW PASS | EVBA+0x10c at 0x8000230c reaches strong USB ISR; 64-byte local packet buffer, register save/restore, acknowledgement read + three NOPs, rete; timer/UART slots intact |
| UART/Print/timing regression | HOST PASS | Existing UART suites at RX sizes 2/64/256, Print/API and both timing suites rerun successfully |
| Installer | PASS | Incomplete/zero/oversized IDs, RC and unsupported USB oscillator rejected before writes; configured/unconfigured WhatIf writes nothing |
| Installed snapshot | PASS | 29 shared-source hashes match canonical sources and manifest; Arduino API copies also match |
| Physical USB clock | HARDWARE NOT TESTED | No frequency/jitter/lock measurement |
| USB enumeration / CDC ACM enumeration | HARDWARE NOT TESTED | No host bus enumeration capture |
| Windows 11 COM port | HARDWARE NOT TESTED | Expected usbser binding documented from Microsoft; no COM port observed |
| SerialUSB TX / RX / Echo | HARDWARE NOT TESTED | No physical payload exchange or sustained duplex test |
| USB disconnect/reconnect / reset / suspend/resume | HARDWARE NOT TESTED | VBUS, bank state, clock and host behavior need board validation |
| DFU → CDC transition | HARDWARE NOT TESTED | Detach/reset/attach implemented; actual re-enumeration pending |
| CDC with Blink running | HARDWARE NOT TESTED | No waveform, ISR latency, stack high-water or timing-under-load measurement |
| Arduino Serial Monitor | HARDWARE NOT TESTED | Usage documented; no board/GUI exchange |
| USB power compliance | HARDWARE NOT TESTED | Clocks intentionally keep running in suspend; board-specific current reduction/measurement remains necessary |

Eight USB host-test executables exercise production portable sources or the
actual Arduino adapter with a fake transport, and the production clock HAL with
a volatile register fixture. They do not emulate USBB FIFO side effects,
physical ACK timing, AVR32 interrupt propagation or the PLL. Reproduce in a
Visual Studio x64 Native Tools prompt from the repository root:

```bat
tests\run-usb-host-tests.cmd
```

Build with a fresh installed package as in [USB_CDC](USB_CDC.md), then:

```powershell
arduino-cli compile --fqbn uc3bduino:avr32:uc3b0256 --build-path build/arduino/USBSerialBlink examples/USBSerialBlink
python tests/verify_firmware.py build/arduino/USBSerialBlink --basename USBSerialBlink.ino
avr32-size build/arduino/USBSerialBlink/USBSerialBlink.ino.elf
```

Final 12 MHz sizes (bytes): `text + data` is occupied content; BIN includes the
startup/vector alignment gap. RAM here includes the reserved 1024-byte stack,
not a measured stack high-water mark.

| Sketch | text / Flash content | data | bss / RAM reservation | BIN span |
| --- | ---: | ---: | ---: | ---: |
| Blink | 1832 | 0 | 1036 | 2256 |
| SerialHello | 2276 | 0 | 1120 | 2700 |
| SerialMillis | 2588 | 0 | 1120 | 3012 |
| USBSerialHello | 6812 | 0 | 1416 | 7236 |
| USBSerialEcho | 6572 | 0 | 1416 | 6996 |
| USBSerialBlink | 7400 | 0 | 1416 | 7824 |

USB examples reserve 392 bytes of static SRAM plus the 1 KiB stack; DPRAM uses
another 208 bytes in the controller, separate from the MCU SRAM figure. Existing
nine Arduino and three bare-metal resource sizes remain unchanged from Stage 5.
The integration fixture is 9048/0/1504 at both 8/16 MHz and 8892/0/1504 on RC.
No hardware test is promoted to PASS by these build/host results.

## Stage 5 – Arduino USB DFU upload

Executed **2026-10-26** on Windows with PowerShell 5.1, AVR32 GCC/G++ 4.3.2,
binutils 2.19.20081016, Arduino CLI 1.5.2-rc.1 and official dfu-programmer 1.1.0
x64. No physical DFU board was present. The 12 MHz OSC0/U0C/PA03 configuration
is a compilation fixture, not a claim about the user's board.

| Check | Status | Actual evidence / limits |
| --- | --- | --- |
| Factory layout/protocol research | PASS | Official AVR32784, AVR32806, AVR32760 and 32059L; pinned tool source inspection, links in DFU_UPLOAD.md |
| Nine Arduino sketches | PASS | ArduinoMinimal, DigitalOutput, Blink, MillisTest, MicrosPulse, SerialHello, SerialMillis, SerialEcho, GpioSmoke compile/link with warnings as errors |
| Default RC regression | PASS | Fresh install without OSC0/Serial route; ArduinoMinimal and Blink compile |
| Three bare-metal builds | PASS | minimal, gpio-toggle, startup-check rebuilt with the same linker |
| ELF/HEX/BIN memory checks | PASS | Entry 0x80002000; no bootloader/user-page load bytes; SRAM word zero reserved; HEX/BIN match ELF; stack top remains 0x8000 |
| Initialized-data fixture | PASS | startup-check data VMA 0x00000004, 8-byte flash initializer checked |
| Upload HEX preflight | PASS | All new Arduino/bare-metal HEX accepted offline; old Stage 4 Blink at 0x80000000 rejected |
| DFU host tests | PASS | 42 checks: malformed/checksum/overlap/out-of-range/entry records, ambiguous devices, identity/version/protection gates, legacy confirmation, device change, fail-fast sequencing |
| Native process boundary | PASS | Synthetic EXE tests argv quoting, stdout/stderr, exits, firmware read lock and erase/flash/launch order; no USB hardware |
| dfu-programmer executable | PASS | Real --version/--help; at32uc3b0256 get product-name returns exit 3, no device present |
| DFU device detection: absent device | PASS | No present 03EB:2FF6 device; wrapper reports DFU device not found before erase |
| DFU device detection: physical identity | NOT TESTED | No positive signature, BOOTPROT or bus/address result from hardware |
| Manual Blink upload | NOT TESTED | Production command prepared; no physical write |
| Flash verify | NOT TESTED | Real command enables default readback; failure propagation host-tested; no physical readback |
| Application start | NOT TESTED | launch/reset sequence prepared; no observed hardware execution |
| Arduino CLI upload recipe/errors | PASS | CLI without --port reaches installed script; missing tool/device each return exit 1 and clear errors |
| Arduino CLI upload on hardware | NOT TESTED | No device to complete programming |
| Arduino IDE board display / Verify / Upload button / recipe / errors | NOT AVAILABLE | No installed IDE 2.x found in app/start-menu checks, usual install paths or running processes |
| Arduino IDE upload on hardware | NOT TESTED | Requires IDE 2.x and a board |
| Blink / GpioSmoke hardware | NOT TESTED | D0 wiring, polarity, waveform and execution remain unmeasured |
| Windows driver binding | NOT TESTED | WinUSB documented from upstream; no driver changed |
| Installer and paths | PASS | Explicit and auto tool paths, spaces, environment lookup, legacy setting, invalid tool rejection, existing-target refusal, WhatIf |
| Shared source manifest | PASS | 21 installed files match canonical source and SHA-256 manifest |

Host tests execute the production functions with synthetic Windows device
enumeration and a native test executable. This validates control flow, not the
Atmel USB protocol or electrical behavior. Reproduce with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/test_dfu_upload.ps1
python tests/verify_firmware.py build/arduino/Blink --basename Blink.ino
.\scripts\test-upload.ps1 -BuildPath build/arduino/Blink -CheckOnly
```

The host script compiles a temporary C# console fixture using the Windows .NET
Framework compiler and removes its own temporary directory afterward.
Representative 12 MHz sizes: Blink **1832 text / 0 data / 1036 bss**, BIN span
**2256** bytes; GpioSmoke **1828 / 0 / 1036**, BIN **2252**; SerialMillis
**2588 / 0 / 1120**, BIN **3012**. All eight pre-existing Arduino sizes and
three bare-metal sizes remain unchanged; flash addresses moved by 0x2000 and
static SRAM by 4 bytes. BSS includes the reserved 1 KiB stack.

Hardware acceptance must record MCU/board marking, bootloader/signature/BOOTPROT,
OSC0 and D0 wiring, WinUSB version, manual/CLI/IDE logs, physical readback,
post-launch USB disappearance and measured Blink/GpioSmoke waveform. Follow
[DFU_UPLOAD](DFU_UPLOAD.md); preserve the factory bootloader. Pending checks
only become PASS after actual execution.

## Historical Stage 4 – HardwareSerial / UART

The following records describe the earlier full-flash builds. Their addresses
and lack of upload support are historical; Stage 5 supersedes that layout.

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
