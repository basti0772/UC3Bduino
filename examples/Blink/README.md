# First DFU hardware test

`Blink.ino` toggles the explicitly configured **D0** test pad every 500 ms
(nominal 1 Hz cycle). There is no assumed `LED_BUILTIN`. Select an available pad
from the actual board schematic with installer `-GpioPort` / `-GpioPin`; compilation
stops if D0 is not configured. Check LED polarity/current-limiting resistor or use
a high-impedance oscilloscope/logic-analyzer input with common ground.

Follow [DFU_UPLOAD](../../docs/DFU_UPLOAD.md) and
[ARDUINO_IDE](../../docs/ARDUINO_IDE.md): install, select the board, open this
sketch, Verify, enter factory DFU, Upload, observe D0. Factory DFU oscillator
requirements apply even when the sketch uses RC. Application start is
`0x80002000`; never upload an old full-flash image.

`GpioSmoke` uses the same pin with nominal 20 ms high/20 ms low (25 Hz) for a
scope. Actual timing depends on the configured oscillator and GPIO/API overhead.
These are prepared tests; no physical Blink or GpioSmoke result is claimed.
