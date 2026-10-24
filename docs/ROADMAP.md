# Roadmap

Each stage should have its own review and validation. A checked build is not a
claim of working peripherals or Arduino compatibility.

1. **Toolchain / Build / Startup** — current stage: AT32UC3B0256 freestanding
   firmware, memory layout, Windows scripts and documented build checks.
2. **Clock / GPIO / Interrupt basics** — reset and clock policy, register access,
   INTC setup, configurable GPIO smoke tests on an identified board.
3. **Arduino GPIO + delay/millis/micros** — first Arduino-facing API and timebase.
4. **Timer / PWM** — timer allocation, output routing and PWM behavior.
5. **HardwareSerial** — UART setup and buffering.
6. **ADC / analogRead** — ADC channels, reference and conversion behavior.
7. **SPI / Wire** — bus APIs and peripheral implementations.
8. **Board variants / pin mapping** — explicit board wiring and more UC3B targets.
9. **Arduino boards.txt / platform.txt integration** — IDE/CLI build integration.
10. **Upload / flashing** — supported probes/loaders and safe memory layout rules.
11. **Compatibility tests** — Arduino API behavior and on-device regression tests.
12. **Boards Manager release** — versioned distributions, checksums and installation.
