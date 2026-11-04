# Minimum hardware acceptance — Stage 8.5

**In progress; release gate is not passed.** The later Stage 8.9 automatic upload
and Stage 9 internal SPI tests passed on 2026-10-30; see [AUTO_UPLOAD](AUTO_UPLOAD.md)
and [Stage 9 test record](TESTING.md#stage-9--spi). IDE GUI, USB RX/Echo and reliable
cold reconnect remain open for the separate public-release gate.
UC3Bduino 0.9.0-dev is experimental.
Observed on 2026-10-28/29, Windows 11, AT32UC3B0256 reference board, external
12 MHz OSC0 crystal, red D1 on PA23 active low. Board identity/wiring comes from
the owner; no instrument-based supply, frequency or analog measurements are claimed.

Source: Stage 8.5 working branch `codex/stage-08-5-public-release`, based on
`4fcd665563449ac7ca91b45a105172a193e6c7e5`. This is an interim report, not
acceptance of a final immutable release commit. Final changes must be rechecked.

| Test | Result | Evidence / boundary |
| --- | --- | --- |
| Factory DFU detection | HARDWARE PASS | One 03EB:2FF6 device; native libusb bus/address pinning; signature 58/20/02/03, legacy bootloader 1.0.2, BOOTPROT=3 |
| DFU firmware upload | HARDWARE PASS | Blink and USBSerialBlink programmed through the checked uploader |
| DFU verify/readback | HARDWARE PASS | dfu-programmer readback validation succeeded; application starts at 0x80002000; bootloader area excluded |
| Application launch | HARDWARE PASS | Launch followed by owner-observed running Blink/USBSerialBlink |
| LED_BUILTIN PA23 | HARDWARE PASS | Owner confirmed red D1 blinking with standard Blink |
| Basic timing sanity | HARDWARE PASS | Owner observed approximately 500 ms on / 500 ms off; no precision measurement |
| Arduino IDE Verify | HARDWARE NOT TESTED | IDE 2.3.10 opens; GUI Verify still pending |
| Arduino IDE Upload | HARDWARE NOT TESTED | CLI/manual DFU pass does not substitute for GUI upload |
| USB CDC enumeration | HARDWARE PASS | After the FIFO correction, Windows binds the regular USBSerialHello firmware as a CDC COM port |
| USB CDC TX | HARDWARE PASS | Regular USBSerialHello emits repeated greetings and increasing millis values at 115200 line coding with DTR set |
| USB CDC RX / Echo | HARDWARE NOT TESTED | Regular Echo hardware test pending |
| USB disconnect/reconnect | HARDWARE NOT TESTED | Working CDC reconnect has not been demonstrated |
| Fresh installation | PASS (software) | Fresh isolated sketchbook, default 12 MHz/PA23, install/diagnose/smoke pass; final GUI/hardware sequence pending |
| HardwareSerial / UART | HARDWARE NOT TESTED | IMPLEMENTED, COMPILE PASS, HOST TEST PASS; nonblocking for this development release |
| ADC / analogRead | HARDWARE NOT TESTED | IMPLEMENTED, COMPILE PASS, HOST TEST PASS; nonblocking for this development release |

Tools: AVR32 GCC/G++ 4.3.2, Arduino CLI 1.5.2-rc.1, dfu-programmer 1.1.0 x64
and its external libusb DLL. An explicitly supplied local VID/PID is used only
in the private test installation. No driver replacement was needed for DFU.

## USB investigation

Private diagnostic sketches preserve the application's protected memory layout.
They are not release examples and do not change the public USB identity defaults.
LED code 9 in the initialization diagnostic confirms foreground entry, OSC0,
PLL1/GCLK3 startup, controller clock and EP0 allocation succeeded, but the host
did not configure the device. LED code 6 in the protocol diagnostic confirms USB
interrupts, bus reset and SETUP processing occurred without a recognized
GET_DESCRIPTOR(device) packet. The packet diagnostic then reported an 8-byte
SETUP with unexpected contents. Changing the FIFO loops to advance byte addresses
resolved enumeration. The production byte-copy helper has a host regression for
SETUP ordering, packet lengths 0..64 and boundaries. All 19 native test executables
pass. This tests byte-copy behavior; it does not emulate the USB peripheral.

The corrected regular USBSerialHello was installed from a fresh source snapshot,
flashed/readback-verified and enumerated successfully. Five consecutive greetings
and millis values 43002..47002 were read over CDC. A transition back to manual
DFU once required physical USB disconnect/reconnect; reliable automatic transition
is part of the next infrastructure task, not yet a pass here.

The checked uploader no longer depends on a debug-only bus/address log line
missing from the official Windows binary. It locates the device through the
tool's external libusb DLL, then pins and rechecks its identity and location for
all operations. Signature, BOOTPROT, application-range and readback checks remain.

## Completion rule

Every minimum item requested for Stage 8.5 must pass on the final candidate:
DFU detection/upload/readback/launch, LED, timing sanity, IDE Verify/Upload,
CDC enumeration/TX/RX/Echo/reconnect, and fresh installation. UART and ADC
hardware tests are explicitly outside this minimum gate. The broader checklist
in [HARDWARE_TEST](HARDWARE_TEST.md) remains useful for later validation.

Do not produce/publish the final release package or mark CDC hardware validated
while this report has a failed or pending minimum item. See [RELEASE_PLAN](RELEASE_PLAN.md).
