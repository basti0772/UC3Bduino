# System clock

The default remains the AT32UC3B0256 internal slow RC oscillator (RCSYS).
Stage 4 adds opt-in OSC0 to supply useful UART baud rates. Board oscillator
population is unknown; external clock settings must be explicitly supplied.

## Configuration and contract

`src/hal/uc3b_clock.h` defaults `UC3B_CLOCK_SOURCE` to
`UC3B_CLOCK_SOURCE_INTERNAL_RC` (1). Source `UC3B_CLOCK_SOURCE_OSC0` (2)
requires `UC3B_OSC0_HZ` and `UC3B_OSC0_CRYSTAL` (0=external clock, 1=crystal).
Other source values produce a compile-time error. There is no assumed crystal
frequency and no PLL implementation.

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

With default RC, no PLL, external oscillator, generic clock, calibration fuse, watchdog, flash
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

## Explicit OSC0 option

At installation, supply both `-Osc0Hz <actual Hz>` and
`-Osc0Mode Crystal` or `-Osc0Mode External`. Accepted frequencies are
400000..16000000 Hz, divisible by 800 so the existing timebase has an exact
100 Hz RC period. Both modes leave CPU=HSB=PBA=PBB=OSC0; getters and Arduino
F_CPU report the supplied frequency. They do not measure it. No runtime clock
change is supported.

After selecting undivided RCSYS, initialization disables OSC0, sets MODE and
STARTUP, enables it and waits for POSCSR.OSC0RDY before selecting MCSEL=OSC0.
The fixed startup setting is 16384 RCSYS cycles, nominally about 142 ms.
OSC0RDY is a startup-counter indication, not a measurement of a correct,
stable external frequency. Verify actual oscillator startup with hardware.
Missing/nonworking OSC0 can stop startup or the CPU at the switch; there is no
fallback that would silently change UART/timer rates. A watchdog may reset the
chip during this wait if fuse settings enable it; watchdog handling is unchanged.

Crystal gain follows the datasheet MODE ranges: G0 below 0.9 MHz, G1 below
3 MHz, G2 below 8 MHz, G3 from 8 MHz. Boundary frequencies select the higher
gain. External-clock mode is MODE=0. The supported 16 MHz ceiling is below
the zero-wait-state flash frequency limit; no flash wait states are changed.
No PLL, OSC1 or OSC32K configuration is added.

OSC0 controls PA18/XIN0 directly through PM, and PA19/XOUT0 in crystal mode;
these are not GPIO A/B/C selections. Serial U0A conflicts with PA18 in either
mode and is rejected. D0 cannot overlap the oscillator pads. Crystal loading,
external-clock voltage/duty cycle and board circuitry remain TODO / VERIFY.
The 12 MHz crystal in [SERIAL](SERIAL.md) is an explicit compile fixture only.

## Verified hardware references

Source: [AT32UC3B datasheet 32059L, January 2012](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32059.pdf),
sections 9.5.1/9.5.2, 9.5.5/9.5.6, 9.6.1–9.6.5, 9.6.12, table 9-6,
sections 28.7.1/28.7.3 and flash timing specifications.
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
| OSCCTRL0 | offset `0x28`; MODE bits 2:0, STARTUP bits 10:8 |
| OSC0 enable / ready | MCCTRL.OSC0EN bit 2 / POSCSR.OSC0RDY bit 7 |

PM has no set/clear aliases for these registers. Its read-modify-write sequences
are serialized during startup. Generated code uses ordinary loads and stores,
not AVR32 memory RMW instructions. GPIO uses atomic aliases separately.

## TODO / VERIFY

- Measure the actual clock and waveform on the identified board. No hardware
  or simulator execution has been performed.
- Confirm silicon revision and calibration/fuses. Section 31.2.4 documents that
  Rev B must not run from a divided slow clock. This configuration is undivided;
  switching from an arbitrary loader state is unsupported.
- OSC0 needs physical frequency/startup verification and revision-specific
  errata review. Future PLL support also requires lock sequencing, frequency
  limits and flash wait states checked together. OSC1 and the 32 kHz oscillator are not direct MCSEL
  choices; their existence does not establish board wiring.
- Stage 3 derives its [timer timebase](TIMING.md) from the PBA getter. It does
  not calibrate RCSYS. The bare-metal smoke-test busy wait remains independent.
