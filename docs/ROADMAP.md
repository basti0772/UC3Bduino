# Roadmap

**Current: Stage 4 – HardwareSerial / UART**

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
5. **ADC / analogRead** — next: ADC channels, reference and conversion behavior.
6. **SPI** — synchronous serial bus API.
7. **Wire / I2C** — two-wire bus API.
8. **PWM / analogWrite** — timer allocation, output routing and PWM behavior.
9. **External interrupts** — explicit pin mapping and interrupt API.
10. **Upload / flashing** — supported probes/loaders and safe memory layout rules.
11. **Compatibility tests** — Arduino API behavior and on-device regression tests.
12. **Boards Manager release** — versioned distributions, checksums and installation.

Board variants and package expansion accompany these stages as verified wiring
and toolchain support become available.
