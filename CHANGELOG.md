# Changelog

## 0.8.0-dev — unreleased development snapshot

- Stage 8: lazy, blocking 10-bit ADC HAL and `analogRead(A0..A7)`, with
  PBA-derived clock, bounded waits and reset/retry on timeout. Failures return -1.
- Shared pin descriptions add ADC channel/mux metadata. D0 retains its configured
  pad and aliases an A-pin when appropriate; A-pins also support digital GPIO.
- AnalogRead and AnalogMultiChannel examples support SerialUSB or HardwareSerial.
- External ADVREF/electrical limits, channel switching, accuracy and pending board
  tests documented. No analogReference, DMA, background sampling or floating point.
- Installer/source package includes the new HAL/examples; ADC host tests join CI.
  No physical ADC/USB/UART pass is claimed.

## 0.7.0-dev — unreleased development snapshot

- Windows installer detects the Arduino sketchbook, validates the external AVR32
  toolchain and pins the local DFU tool path. Existing installations are preserved.
- Installed examples appear under the board's UC3BduinoExamples library.
- Compile-only smoke tests, PnP-only diagnostics and checksum-verified optional
  dfu-programmer download; no implicit upload or driver installation.
- Version macros, source archive/checksums, Windows installation instructions,
  troubleshooting and hardware test checklist.
- Static packaging and native host tests in CI. AVR32 builds remain local.
- Existing GPIO, timing, HardwareSerial, protected factory DFU upload and native
  USB CDC code retained. Physical board/USB tests remain pending.

Earlier platform milestones were development work, not published releases:

| Platform milestone | Implemented functionality |
| --- | --- |
| Stage 1–2.5 | AVR32 build/startup, GPIO, minimal Arduino core |
| Stage 3 | millis/micros/delay/delayMicroseconds |
| Stage 4 | HardwareSerial and decimal Print |
| 0.5.0 (Stage 5) | Protected application layout and factory DFU upload wrapper |
| 0.6.0 (Stage 6) | SerialUSB / USB CDC ACM |

Versioning follows Semantic Versioning: `0.x` is experimental development;
`-dev` denotes an unreleased snapshot. Changes during `0.x` may break APIs or
configuration; record them here. `1.0.0` is reserved for the first stable,
hardware-validated core. Keep `VERSION`, platform/library metadata and
`UC3BduinoVersion.h` synchronized (`tests/check_package.py` enforces this).
The board identifier remains **uc3bduino:avr32:uc3b0256**.
