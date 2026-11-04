# UC3Bduino reference board

Stage 8.5 defines the board described by the owner, rather than a generic UC3B
breakout. Check these wiring assumptions before using this variant on another board.

| Item | Reference-board configuration |
| --- | --- |
| MCU | AT32UC3B0256 |
| Main clock | External OSC0, 12 MHz crystal on PA18/PA19 |
| CPU / HSB / PBA / PBB | 12 MHz, undivided |
| USB clock | OSC0 -> PLL1 -> 48 MHz GCLK3 |
| Onboard LED | Schematic D1, red, PA23, active low |
| LED Arduino number | `LED_BUILTIN = 9`, fixed PA23 |
| Test output | `D0 = 0`, defaults to PA23; may be overridden independently |
| Analog names | A0..A7 retain Arduino numbers 1..8; see [ADC](ADC.md) |
| USB | Native device, factory DFU bootloader; application CDC implemented |
| Application origin | `0x80002000`; protected factory boot area is `0x80000000..0x80001FFF` |

The board's schematic label **D1** is the LED component designation, not an
Arduino digital-pin number. `LED_BUILTIN` always maps to PA23 even when D0 is
overridden. The two names alias the same physical pad in the default configuration.

```cpp
pinMode(LED_BUILTIN, OUTPUT);
digitalWrite(LED_BUILTIN, LED_BUILTIN_ON);  // LOW, red LED on
digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF); // HIGH, red LED off
```

`LED_BUILTIN_ACTIVE_LOW` is 1. Ordinary `digitalWrite(pin, HIGH)` still drives
the pad high; polarity is handled by the example, not by the GPIO implementation.
Output initialization starts LOW, so the LED can briefly light before setup
writes OFF. Blink uses 500 ms ON and 500 ms OFF without extra pin parameters.

The Arduino installer defaults to this crystal and PA23. `-GpioPort` and
`-GpioPin` together override only D0. Paired `-Osc0Hz` / `-Osc0Mode` options
support explicitly configured alternative hardware. `-InternalRc` selects the
nominal 115.2 kHz internal RC for diagnostic/compile configurations; it cannot
support application USB. Bare-metal examples retain their separate RC default.

UART has no default route. U0A conflicts with OSC0 and U1A uses the onboard
PA23 LED; the reference-board installer rejects these conflicts. Other UART
routes require checking actual connectors and circuitry. ADC reference voltage,
connector numbering and exact supply measurements have not been established by
this board definition. The reference MCU was identified as revision I
(`DID=0x81EE403F`) during Stage 8.9 diagnostics.

Stage 9 appends logical MOSI=10/PA14, MISO=11/PA25, SCK=12/PA15 and SS=13/PA16.
These preserve all existing logical indices. SPI uses peripheral function A on
the three bus pads; SS is ordinary GPIO. Internal loopback passed, but connector
routing and external SPI devices remain untested. See [SPI](SPI.md).

Stage 10 adds logical SDA=14/PA10 and SCL=15/PA09 for a GPIO software I2C master.
It requires external pull-ups and does not use native TWI. Existing pin indices
stay unchanged; no external I2C hardware has been validated. See [WIRE](WIRE.md).

USB VID/PID defaults remain zero: SerialUSB stays disabled/detached until an
explicit local identity is configured. Local hardware-test IDs are not product
defaults and are not included in source releases. See [USB_CDC](USB_CDC.md).

Actual hardware results, including the corrected CDC enumeration failure and
the remaining tests, are recorded in [HARDWARE_ACCEPTANCE](HARDWARE_ACCEPTANCE.md).
