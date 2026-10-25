# Roadmap

**Current: Stage 2.5 – Arduino IDE compilation smoke test**

Each stage should have its own review and validation. A checked build is not a
claim of working peripherals or Arduino compatibility.

1. **Toolchain / Build / Startup** — build verified: AT32UC3B0256 freestanding
   firmware, memory layout, Windows scripts and documented build checks.
2. **Clock / GPIO / hardware basics** — build verified: internal
   RC clock, small GPIO HAL and configurable smoke test. Hardware validation is
   pending; interrupts remain masked.
   **Stage 2.5 – Arduino IDE compilation smoke test** — current intermediate
   stage, CLI build verified: minimal Arduino.h, setup/loop main,
   pinMode/digitalWrite/digitalRead, boards.txt, platform.txt and configurable
   D0 variant mapping. Arduino CLI discovers the board and builds both .ino
   examples to ELF/HEX/BIN/map using AVR32-GCC. IDE GUI and hardware are not
   tested; see [TESTING](TESTING.md).
3. **Interrupts and hardware-timer timebase** — next stage: choose a suitable
   AT32UC3B timer, define ownership/INTC behavior and implement real `delay()`,
   `delayMicroseconds()`, `millis()` and `micros()`. Validate timing and rollover
   on hardware before expanding the Arduino API. Not started in Stage 2.5.
4. **Timer / PWM** — timer allocation, output routing and PWM behavior.
5. **HardwareSerial** — UART setup and buffering.
6. **ADC / analogRead** — ADC channels, reference and conversion behavior.
7. **SPI / Wire** — bus APIs and peripheral implementations.
8. **Board variants / pin mapping** — explicit board wiring and more UC3B targets.
9. **Arduino package expansion** — harden the initial IDE/CLI integration.
10. **Upload / flashing** — supported probes/loaders and safe memory layout rules.
11. **Compatibility tests** — Arduino API behavior and on-device regression tests.
12. **Boards Manager release** — versioned distributions, checksums and installation.
