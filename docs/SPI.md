# Stage 9 — SPI master

Experimental, single-bus SPI master for the AT32UC3B0256. The reference board has
a 12 MHz crystal and revision I silicon. Internal hardware loopback passed;
external bus wiring, signal timing and slave-device operation remain untested.

## API and pins

Include `<SPI.h>` and use the provided `SPI` object:

```cpp
#include <SPI.h>
void setup() {
    pinMode(SS, OUTPUT);
    digitalWrite(SS, HIGH);
    SPI.begin();
}
void loop() {
    SPI.beginTransaction(SPISettings(1000000UL, MSBFIRST, SPI_MODE0));
    if (SPI) {
        digitalWrite(SS, LOW);
        uint8_t received = SPI.transfer((uint8_t)0x55);
        digitalWrite(SS, HIGH);
        (void)received; // Interpret according to the attached device's protocol.
    }
    SPI.endTransaction();
    delay(100);
}
```

| Name | Arduino logical index | MCU pad | Peripheral function |
| --- | --- | --- | --- |
| MOSI | 10 | PA14 | A (0) |
| MISO | 11 | PA25 | A (0) |
| SCK | 12 | PA15 | A (0) |
| SS | 13 | PA16 | Ordinary GPIO, controlled by sketch |

These are MCU pads, not connector or package pin numbers. D0, A0–A7 and
LED_BUILTIN keep their previous indices and mappings. `SPI.begin()` configures
only MOSI/MISO/SCK; it does not drive SS or any NPCS pad. The sketch configures,
asserts and deasserts each device's GPIO chip select, including on failure.
`endTransaction()` does not toggle CS. `SPI.end()` resets SPI and releases only
the three bus pads as floating GPIO inputs; the PBA clock remains enabled.

The route does not overlap the PA23 LED, PA18/19 crystal or the current ADC
mapping. A custom D0 can alias a bus pad. Do not use GPIO or another peripheral
on these pads while SPI owns them; there is no general pin ownership manager.
Check the board schematic and VDDIO/slave voltage compatibility before wiring;
this stage does not establish connector routing or 5 V tolerance. Even internal
loopback drives MOSI/SCK and requires the SPI pin multiplexing to be enabled.

Supported calls:

- `SPI.begin()`, `SPI.end()`; repeated successful begin is idempotent, not refcounted.
- `SPI.beginTransaction(SPISettings(clock, bitOrder, dataMode))`, `endTransaction()`.
  Transactions are foreground-only and cannot nest. End retains the selected
  settings; it does not restore a previous device's configuration.
- `transfer(uint8_t)`, `transfer16(uint16_t)`, `transfer(void *, size_t)`.
  Buffer transfers replace bytes in place. Null with count zero is a no-op;
  null with nonzero count is rejected. A 16-bit transfer uses two 8-bit frames,
  high byte first for MSBFIRST and low byte first for LSBFIRST, independent of
  CPU endianness. There may be an inter-byte gap.
- `MSBFIRST`, `LSBFIRST`, `SPI_MODE0..SPI_MODE3`.
- UC3Bduino extensions: `lastError()`, `clearError()`, `clockHz()`, boolean status,
  and `setLoopback(bool)` outside a transaction for controller diagnostics.

Use local `SPISettings` values (e.g. the temporary in the example). This core's
existing startup rejects global C++ constructors. The `SPI` object itself is
trivial BSS data. The implementation is integrated into the core like Serial;
no separate library download or installer flag is required. This also avoids
Arduino CLI's missing-header discovery incompatibility with legacy GCC 4.3.2.

Slave mode, DMA, asynchronous transfers, hardware-controlled CS, alternate pin
routes, legacy `setClockDivider/setDataMode/setBitOrder`, interrupt registration
and SPI access from ISRs are not supported. `SPI_HAS_TRANSACTION` is not defined:
Arduino uses it to promise `usingInterrupt()` as well as the transaction calls.
Libraries depending on these additional facilities need adaptation. No claim of
SD-card, display or sensor-library compatibility is made.

## Controller, timing and error policy

The HAL uses the real SPI block at `0xFFFF2400`, PBA mask bit 5, fixed internal
CSR0 selection and 8-bit frames. No interrupt vector, USB/PLL setting, bootloader
or startup change is involved. SPI transfer waits keep the caller's interrupt
mask unchanged, allowing timer and USB service during normal foreground use.

`SCBR = ceil(PBA/requested_hz)`, with a conservative minimum divider of 2 and
maximum of 255. This avoids SCBR=0 and the documented SCBR=1/multiple-CS erratum.
A request too slow to represent, zero frequency, invalid mode/order or invalid
clock metadata fails before applying settings. No silent rounding above the
requested rate is permitted. At 12 MHz PBA the maximum is 6 MHz; the minimum
integer request is 47,059 Hz. `clockHz()` reports the integer floor of PBA/SCBR.
This is a calculated clock, not a physical frequency measurement.

The UC3B phase bit is **NCPHA**, the inverse of ordinary CPHA:

| Arduino mode | CPOL | NCPHA |
| --- | --- | --- |
| 0 | 0 | 1 |
| 1 | 0 | 0 |
| 2 | 1 | 1 |
| 3 | 1 | 0 |

The controller shifts MSB first; LSBFIRST reverses each outgoing and incoming
byte in software. Mode-fault detection is disabled (`MODFDIS=1`) because this is
a single-master bus with software CS. CSAAT and DMA are unused. The driver
checks SPIENS as well as TDRE because disabled SPI can still report TDRE on
affected silicon. Each byte waits for receive completion and TXEMPTY before
returning, so the sketch can safely deassert CS afterwards.

Waits use wrap-safe CPU COUNT differences and a separate 100,000-poll limit if
COUNT stops. Transfer allowance is one full byte plus a nominal 10 ms margin;
clock-enable wait has a nominal 10 ms bound. Each buffer element is bounded;
total buffer duration scales with its count. Hardware errors/timeouts reset the
controller, release its bus pads and require `SPI.begin()` to recover. No failed
HAL transfer modifies the receive output. The Arduino byte/word APIs return zero
on failure: check `lastError()` to distinguish it from valid received zero.

Errors are `UC3B_SPI_OK`, `NOT_STARTED`, `ARGUMENT`, `CONTEXT`, `TIMEOUT`, or
`HARDWARE` (all prefixed `UC3B_SPI_`). `clearError()` only clears the diagnostic;
it does not apply rejected settings or restart a stopped controller. End the
transaction and begin/configure again after fixing the cause. On buffer failure,
only the successfully received prefix is replaced; the remaining suffix is
untouched. Deselect the slave and retry its complete protocol operation, since
some bytes may already have reached it.

## Verification

The native tests execute the actual HAL against fake register access and the
actual C++ API against a fake HAL. They cover divider boundaries and overflow,
all four mode encodings, all 256 byte values in both bit orders, captured wire
byte order, invalid settings, repeated/nested lifecycle, buffer failure, clock
and transfer timeouts, COUNT wrap/stall, disabled hardware and recovery.
They do not establish external electrical correctness.

`examples/SPILoopback` checks all byte values, a word and an in-place buffer
across four modes, two orders and three requested rates (100 kHz, 1 MHz, 4 MHz),
plus a controller end/begin cycle: **25 cases**. It releases SPI after testing
and repeats the result over CDC while blinking the LED. No external jumper or
slave is needed. Loopback cannot independently establish external bit order,
clock polarity/phase, actual clock rate, pad voltage or slave timing.

2026-10-30 first hardware run: **25 cases, 0 failures**, unique build
`735072ca-7bd6-4218-93a5-81a63d075825`; automatic CDC → factory DFU → protected
flash/readback → CDC passed. Local evidence: `.scratch/stage09/hardware.log`
and `hardware-cdc.log`. The final build
`03188aa4-236b-4359-9069-6821b626ef75` repeated **25 cases, 0 failures** after
the invalid-settings recovery refinement (`hardware-final.log`,
`hardware-final-cdc.log`). A subsequent full AutoFlashTest upload from that
running SPI sketch also passed. Final regression results and commands are in
[TESTING](TESTING.md).

`examples/SPIMaster` is a compile-tested external-bus template. The owner reported
no SPI wiring/device; no external loopback or peripheral hardware pass is claimed.
Other chips/clocks/revisions, UART/ADC hardware, IDE GUI and cold reconnect remain
outside this acceptance. The earlier USB reconnect descriptor error remains open.

## References

- [AT32UC3B complete datasheet 32059L](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32059.pdf):
  table 4-1, clock map, §§18.6–18.8 and §31.2 silicon errata. Register/pin layout is
  checked at compile time against the separately installed `spi_199.h` headers.
- [Arduino SPI API reference header](https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/SPI/src/SPI.h):
  public call signatures, mode constants and feature-macro meaning only. No AVR
  implementation or ASF source was copied; this implementation is original MIT code.
