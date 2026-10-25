# System clock

Stage 2 uses the AT32UC3B0256 internal slow RC oscillator (RCSYS). No external
crystal is needed for the GPIO smoke test. Board oscillator population is unknown.

## Configuration and contract

`src/hal/uc3b_clock.h` defaults `UC3B_CLOCK_SOURCE` to
`UC3B_CLOCK_SOURCE_INTERNAL_RC`. Other values produce a compile-time error.
This is the extension point for a later board-verified OSC0/PLL configuration;
there is no external-clock implementation or assumed crystal frequency.

Call `uc3b_clock_init()` from `main`, after reset startup and before GPIO access.
Interrupts must remain masked. This is a reset-time initialization API, not a
live clock-switching service or bootloader handoff contract. PM and the PBA bridge
must still be accessible, as they are after hardware reset.

Initialization selects RCSYS in `MCCTRL.MCSEL`, preserving oscillator enable bits.
`CKSEL=0` selects undivided CPU, HSB, PBA and PBB clocks. Reset already provides
that value, so no divider write is needed on the normal reset path. If a divider
change is needed, the code waits for `POSCSR.CKRDY` before and after writing. It
enables GPIO in `PBAMASK` without disabling other modules and waits for
`POSCSR.MSKRDY` before returning. Polling has no timeout; use a debugger to
diagnose a stalled hardware handshake.

No PLL, external oscillator, generic clock, calibration fuse, watchdog, flash
wait-state or interrupt configuration is changed. Existing flash wait states are
safe at this slow clock. No sleep mode is entered.

| API | Configured nominal result |
| --- | ---: |
| `uc3b_clock_get_cpu_hz()` | 115200 Hz |
| `uc3b_clock_get_hsb_hz()` | 115200 Hz |
| `uc3b_clock_get_pba_hz()` | 115200 Hz |
| `uc3b_clock_get_pbb_hz()` | 115200 Hz |

These getters report the nominal value from the installed official
`AVR32_PM_RCOSC_FREQUENCY` definition. They are not measurements and do not
decode arbitrary changes made by other code. Their contract requires successful
initialization and continued HAL ownership of clock configuration. RCSYS varies
with temperature, supply and calibration; table 28-16 characterizes this variation.
Do not use these values to promise precise baud rates or elapsed time.

## Verified hardware references

Source: [AT32UC3B datasheet 32059L, January 2012](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32059.pdf),
sections 9.5.1, 9.5.5/9.5.6, 9.6.1–9.6.3, 9.6.12, table 9-6 and section 28.7.1.
Definitions come from installed `uc3b0256.h` / `pm_230.h`;
see [header provenance](THIRD_PARTY.md).

| Register / field | Verified value or meaning |
| --- | --- |
| PM base | `0xFFFF0C00` |
| MCCTRL | offset `0x00`, reset `0`; MCSEL bits 1:0, RCSYS = 0, OSC0 = 1, PLL0 = 2 |
| CKSEL | offset `0x04`, reset `0`; all DIV/SEL fields zero = no division |
| CPU / HSB | HSB division follows CPU division on this MCU |
| PBAMASK | offset `0x10`; bit 1 enables GPIO; module clocks enabled at reset |
| Header GPIO clock index | 65 = PBA domain (2) × 32 + bit 1 |
| POSCSR | offset `0x54`; CKRDY bit 5, MSKRDY bit 6 |

PM has no set/clear aliases for these registers. Its read-modify-write sequences
are serialized during startup. Generated code uses ordinary loads and stores,
not AVR32 memory RMW instructions. GPIO uses atomic aliases separately.

## TODO / VERIFY

- Measure the actual clock and waveform on the identified board. No hardware
  or simulator execution has been performed.
- Confirm silicon revision and calibration/fuses. Section 31.2.4 documents that
  Rev B must not run from a divided slow clock. This configuration is undivided;
  switching from an arbitrary loader state is unsupported.
- Future OSC0/PLL support needs board frequency, oscillator mode/startup time,
  ready/lock sequencing, frequency limits, flash wait states and revision-specific
  errata checked together. OSC1 and the 32 kHz oscillator are not direct MCSEL
  choices; their existence does not establish board wiring.
- Stage 3 derives its [timer timebase](TIMING.md) from the PBA getter. It does
  not calibrate RCSYS. The bare-metal smoke-test busy wait remains independent.
