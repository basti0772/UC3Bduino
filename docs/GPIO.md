# GPIO hardware layer

`src/hal/uc3b_gpio.[ch]` is an internal AT32UC3B0256 interface. It has no Arduino
pin numbers, board LED assumption, interrupt dispatcher or ASF dependency.
Call `uc3b_clock_init()` before accessing GPIO.

## Port and pin representation

`Uc3bGpioPin` contains `uint8_t port` and `uint8_t pin`. Ports are GPIO register
banks, not package pin numbers. For this 44-GPIO device the mapping is:

| HAL port | Pin range | Device signal | Datasheet GPIO number |
| --- | --- | --- | --- |
| `UC3B_GPIO_PORT_A` (0) | 0–31 | PA00–PA31 | 0–31 |
| `UC3B_GPIO_PORT_B` (1) | 0–11 | PB00–PB11 | 32–43 |

`uc3b_gpio_is_valid()` checks these bounds without accessing hardware. All
mutators return `false` for invalid pins, with no register access or invalid bit
shift; `read()` also returns `false`. Call `is_valid()` if LOW and invalid must be
distinguished. Validate wider external values before narrowing into the two
8-bit fields. A valid pin does not mean it is safe or available on a given board.

PA00/PA01/PA02 also carry TDI/TDO/TMS. JTAG can override GPIO irrespective of
GPER. Their GPIO/peripheral mux bits are not implemented like ordinary muxed
pins (the official header's GPER implementation mask excludes bits 0–2).
The HAL accepts them as documented GPIO-capable pads but does not disable JTAG;
the smoke-test configuration excludes them. TCK is not a GPIO. Enabled Nexus
trace can also override pads. Check the schematic and debugger/trace ownership.

## Operations

| Function | Behavior |
| --- | --- |
| `config_output(pin)` | Disable pin interrupt and driver, preload LOW, disable pull-up, select GPIO, enable output |
| `config_input(pin)` | Disable pin interrupt, output driver and pull-up; select GPIO (floating input) |
| `enable_gpio(pin)` | Set GPER only; preserve direction, latch and pull-up state |
| `write(pin, value)` | Set or clear the output latch according to the boolean value |
| `set(pin)` / `clear(pin)` / `toggle(pin)` | Write OVRS / OVRC / OVRT |
| `read(pin)` | Return the sampled pad value from PVR, not the output latch |
| `enable_pullup(pin)` / `disable_pullup(pin)` | Write PUERS / PUERC |

Names in the table have the `uc3b_gpio_` prefix. A successful mutator returns
`true`; this reports an accepted operation, not a measurement of the pad.
Writes affect the latch even when the driver is disabled. Configuration resets
the pull-up; enable it explicitly after `config_input()` if wanted.
Repeated `config_output()` returns the output to LOW.

Set/clear/toggle use single-bit masks and hardware alias writes, preserving other
pins without software read-modify-write. A configuration sequence touches several
registers and is not atomic. Serialize configuration and ownership changes;
same-pin competing writers are not arbitrated. Output configuration preloads LOW
before enabling the driver but can briefly release an already driven pin; it
does not promise a glitch-free peripheral handoff.

PVR sampling requires GPIO ownership (GPER=1) or the pin interrupt enabled;
configure GPIO before reading. Sampling has clock latency; an immediate read
after a write need not show the new level. The example waits before observing
PVR. There is no debounce, glitch-filter API or GPIO interrupt API in this stage.

## Register and mux references

The [AT32UC3B datasheet 32059L](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32059.pdf),
table 3-1, tables 4-1/4-2/4-3, sections 5.1/5.4 and chapter 17, is the primary
reference. Official definitions are in installed `uc3b0256.h` / `gpio_110.h`.

GPIO's PBA base is `0xFFFF1000`, with two register banks at stride `0x100`.
`uc3b_device.h` checks the official C layout at compile time. The local/fast GPIO
address is not used; it has silicon revision limitations (section 31.2.4).

| Register aliases used | Offset within bank |
| --- | --- |
| GPERS | `0x04` |
| ODERS / ODERC | `0x44` / `0x48` |
| OVRS / OVRC / OVRT | `0x54` / `0x58` / `0x5C` |
| PVR | `0x60` |
| PUERS / PUERC | `0x74` / `0x78` |
| IERC | `0x98` |

Reset enables GPIO for implemented pads (`GPER=0xFFFFFFFF` in bank 0,
`0x00000FFF` in bank 1); ODER, OVR and PUER reset to zero (table 17-4).
JTAG pads have special electrical behavior described in section 5.1.

For ordinary muxed pins, GPER=1 selects GPIO and GPER=0 selects the peripheral
chosen by PMR0/PMR1. The A/B/C assignment is pin-specific (table 4-1); its D
column applies only to UC3Bx512 and must not be used for this target. This stage
exposes `enable_gpio()`, not a generic peripheral selector. Oscillator and
debug/trace pin control are separate from this ordinary peripheral mux.

Internal pull-ups are supported. There is no internal pull-down API. Although
the shared header has ODMER fields, the UC3B0256 header marks that feature
unimplemented; this HAL does not access it or promise a GPIO open-drain mode.

## Example and hardware verification

A syntax-only example for PA03 (not a board recommendation):

```c
Uc3bGpioPin pin;
pin.port = UC3B_GPIO_PORT_A;
pin.pin = 3;
uc3b_clock_init();
uc3b_gpio_config_output(pin); /* LOW */
uc3b_gpio_set(pin);
uc3b_gpio_clear(pin);
uc3b_gpio_toggle(pin);
uc3b_gpio_config_input(pin);
uc3b_gpio_enable_pullup(pin);
/* Allow the pad to settle before reading it. */
```

Scalar initialization avoids a GCC 4.3 freestanding aggregate-initializer
`memcpy` dependency. Applications can use their own verified board mapping.
See the [configurable toggle example](../examples/gpio_toggle/README.md).

TODO / VERIFY on hardware: confirm a safe pad from the schematic; check output
LOW/HIGH and repeated toggles with a scope, unchanged neighboring pins, floating
input with external drive, input with pull-up, and PVR readback. Check both GPIO
banks if accessible. No physical GPIO test has been performed.
