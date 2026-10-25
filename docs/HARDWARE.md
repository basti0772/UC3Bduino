# AT32UC3B0256 hardware facts

Primary references (consulted 2026-10-24):

- Microchip/Atmel [AT32UC3B complete datasheet, 32059L, January 2012](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32059.pdf):
  table 6-3 (CPU system registers), table 6-4 (event entries), table 6-5
  (architecture revision), sections 7.1/7.2 (memory), 8.1/8.2 (reset boot).
- Atmel [AVR32 Architecture Manual, 32000D, April 2011](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32000.pdf):
  sections 2.10/2.11 (SR and EVBA), 8.3.1 (AVR32A events), instruction descriptions.

| Item | Verified value |
| --- | --- |
| Flash | `0x80000000`, 256 KiB (`0x40000`) |
| SRAM | `0x00000000`, 32 KiB (`0x8000`) |
| Reset fetch | `0x80000000` |
| CPU architecture | AVR32 UC, revision 1 / AVR32A |
| EVBA system register address | `4` (system register, not a memory-mapped peripheral) |
| Global interrupt mask | SR bit 16 |
| Exception mask | SR bit 21 |

The project initializes SP to SRAM's exclusive upper end, `0x8000`, and reserves
the top 1024 bytes. This stack size is a project choice, not a device property.
Linker assertions reject overlap with static data. There is no heap or runtime
stack-overflow detection.

Reset code sets GM, initializes SP and EVBA, clears EM, copies `.data` from its
flash load address in words, clears `.bss` in words, and calls `main`.
Linker alignment includes any trailing padding in the initialization bounds.
Both empty sections and nonempty sections are supported. A returning `main`
enters `__main_returned`, an infinite loop.

The separate exception table is 512-byte aligned, covering fixed offsets from
`0x00` through `0x100`. It contains instructions that loop at the offending entry,
not handler addresses. Offsets and unused architectural slots are named beside
the assembly. Reset and exception entry zero are separate: an unrecoverable
exception must not silently restart initialization.

No INTC registers or peripheral interrupt vectors are initialized. GM stays set.
The current EVBA alignment/layout applies to the fixed exception offsets only;
A future interrupt stage must review INTC autovectors before enabling interrupts.
Stage 2 adds two NOPs after `ssrf GM` for the Rev B erratum in section 31.2.4.
The rest of the reset runtime and the linker script are retained.

Clock and GPIO facts, reset values, register offsets and special pad ownership
are documented in [CLOCK](CLOCK.md) and [GPIO](GPIO.md), cross-checked against
the installed official [device headers](DEVICE_HEADERS.md). No register structs
or vendor implementation sources are copied into the HAL.

## TODO / VERIFY on real hardware

- Confirm the actual chip marking and silicon revision and review its errata.
  The `uc3b0256es` engineering-sample target is not selected.
- Verify reset-to-main execution, SP, copy/zero loops and trap behavior with JTAG.
- Check existing bootloader, flash protection and watchdog fuse configuration.
  This image begins at the first flash byte; no resident bootloader is reserved.
- Reset uses the internal RC clock; the clock HAL explicitly selects undivided
  RCSYS and enables the GPIO bus clock. It does not service a watchdog or promise
  timing accuracy. Board oscillator population remains unknown.
- Revalidate reset assumptions if a loader jumps to `_start` without a hardware
  reset. Such a handoff is not currently supported.

The GPIO example requires an explicit pin selected from the board schematic.
Building and inspecting an ELF does not verify execution on a physical
AT32UC3B0256. No on-device test has been performed.
