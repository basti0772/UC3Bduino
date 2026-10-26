# Configurable GPIO smoke test

This bare-metal test initializes the internal RC clock, configures one output
initially LOW, and repeatedly toggles it. There is no default LED or output pin.
Use the board schematic to choose a free, safe pad first.

Build interface (replace placeholders; they are intentionally not valid values):

```powershell
.\scripts\build.ps1 -Example gpio-toggle -GpioPort <A-or-B> -GpioPin <pin-number>
```

For **compilation only**, PA03 was used in validation:

```powershell
.\scripts\build.ps1 -Example gpio-toggle -GpioPort A -GpioPin 3
python tests/verify_firmware.py build/at32uc3b0256/gpio-toggle
```

This does not identify PA03 as an LED, an available connector, or a safe output
on your board. Do not flash that configuration until its wiring is verified.
PA03–PA31 and PB00–PB11 are accepted; PA00–PA02 are excluded to preserve JTAG.
Missing or invalid parameters fail before compiling. Direct Make builds must
provide `APP_SOURCE=examples/gpio_toggle/main.c` and `APP_DEFINES` containing
`-DTEST_GPIO_PORT=0|1 -DTEST_GPIO_PIN=n`; C preprocessing also rejects missing or
invalid choices. The port macro is numeric, not the letter A or B.

Artifacts are in `build/at32uc3b0256/gpio-toggle/`. Rebuilding with another pin
replaces that example's artifacts; check the compiler's printed defines.
No programming command is supplied or run.

`-BusyWaitIterations 10000` is the default; a positive integer up to 100000000
changes the count. Each toggle is followed by one busy wait before PVR sampling
and another before the next toggle, without claiming a calibrated period.

> Temporary busy wait for hardware smoke test only.
> Will be replaced by timer-based timing in a later stage.

Iterations are not milliseconds. There is no `delay()` API, timer setup or
timebase. RC frequency varies; compiler settings affect the loop.

## Local hardware procedure (not yet executed)

1. Confirm AT32UC3B0256, silicon revision, board power, flash/bootloader layout,
   watchdog fuses and programmer/debugger. Keep a copy of existing firmware.
2. Choose a pad with no conflicting driver, debugger or oscillator use. Connect
   a scope with shared ground, or a correctly limited LED circuit.
3. Build with that pin and program using your verified local workflow. Begin
   with hardware reset, not a jump from an unknown loader state.
4. Break after initialization: `gpio_test_ready` should be true; nominal
   `gpio_test_cpu_hz`, `gpio_test_pba_hz`, `gpio_test_pbb_hz` should each be
   115200. Inspect MCCTRL.MCSEL=0, CKSEL=0 and PBAMASK bit 1 set.
5. Run and observe repeated LOW/HIGH transitions at the physical pad. Check
   `gpio_test_level` after its sampling wait. Record measured timing; no
   particular LED period is promised.
6. For input/pull-up/set/clear checks follow [GPIO.md](../../docs/GPIO.md) with
   a board-specific test program; the toggle example drives its one pin.

**Build tested:** yes, compiled and linked with AVR32-GCC 4.3.2.
**Hardware tested:** no; no board or simulator was available.
