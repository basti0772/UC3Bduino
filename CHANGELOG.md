# Changelog

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
