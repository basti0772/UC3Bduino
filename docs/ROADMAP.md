# Roadmap

**Current: Stage 10 – Wire / software I2C master (0.9.0-dev)**

Each stage should have its own review and validation. A checked build is not a
claim of working peripherals or Arduino compatibility.

1. **Toolchain / Build / Startup** — build verified: AT32UC3B0256 freestanding
   firmware, memory layout, Windows scripts and documented build checks.
2. **Clock / GPIO / hardware basics** — build verified: internal
   RC clock, small GPIO HAL and configurable smoke test. Hardware validation is
   pending; interrupts remain masked.
   **Stage 2.5 – Arduino IDE compilation smoke test** — intermediate
   stage, CLI build verified: minimal Arduino.h, setup/loop main,
   pinMode/digitalWrite/digitalRead, boards.txt, platform.txt and configurable
   D0 variant mapping. Arduino CLI discovers the board and builds both .ino
   examples to ELF/HEX/BIN/map using AVR32-GCC. IDE GUI and hardware are not
   tested; see [TESTING](TESTING.md).
3. **Arduino timing foundation** — build and host logic tests verified: TC0,
   minimal INT0 entry, millis/micros and timer-based delays. All five Arduino
   sketches compile. Hardware accuracy and interrupt latency remain untested.
4. **HardwareSerial / UART** — build and host logic tests verified: one Serial,
   selectable MCU routes, polling TX, INT1 RX ring, decimal Print and explicit
   OSC0 option. All eight Arduino sketches compile. Real board pin/clock selection
   and physical UART tests are pending; see [SERIAL](SERIAL.md).
5. **Arduino USB DFU upload** — factory bootloader reservation, checked
   dfu-programmer workflow, shared manual/CLI/IDE recipe and Blink procedure.
   Local build/host checks are recorded separately from pending board tests in
   [TESTING](TESTING.md); see [DFU_UPLOAD](DFU_UPLOAD.md).
6. **USB CDC / SerialUSB** — build and host logic verified: separate PLL1/GCLK3
   at 48 MHz, USBB device driver, EP0/CDC ACM, static rings and shared Print API.
   VID/PID must be supplied by the developer. Physical enumeration/Windows COM,
   TX/RX and DFU transition remain untested; see [USB_CDC](USB_CDC.md).
7. **Packaging / easy installation** — Windows installer, common compiler
   detection, diagnostics, bundled examples, source archives/checksums and host CI.
   See [PACKAGING](PACKAGING.md). Boards Manager/tool redistribution and release
   publication remain deferred until verified dependencies and hardware tests exist.

8. **ADC / analogRead** — lazy ADC HAL, native 10-bit reads, verified A0–A7,
   bounded waits, examples and host/build checks. External ADVREF; physical
   accuracy/settling still untested. See [ADC](ADC.md).
   **Stage 8.9 – Automatic USB reboot/upload** — repeated reference-board cycles
   and Arduino CLI upload passed; opt-in GP31/watchdog path preserves factory DFU.
   See [AUTO_UPLOAD](AUTO_UPLOAD.md).
9. **SPI** — master begin/end, foreground transactions, four modes, both bit orders,
   byte/word/buffer transfers, bounded failures. Internal reference-board loopback
   passed; external bus/slave operation untested. See [SPI](SPI.md).
10. **Wire / I2C** — foreground GPIO software master, 32-byte buffers, real probes,
    repeated START, clock stretching and bounded waits. External bus untested;
    native TWI offload remains deferred. See [WIRE](WIRE.md).
11. **PWM / analogWrite** — verified timer/output mapping.
12. **External interrupts / attachInterrupt** — application interrupt API.
13. **EEPROM / persistent storage** — storage strategy with flash protections.
14. **Additional UARTs / refinements** — expand verified serial support.

USB improvements, wider Arduino compatibility and hardware regression tests
continue alongside these stages.

Board variants and package expansion accompany these stages as verified wiring
and toolchain support become available.
