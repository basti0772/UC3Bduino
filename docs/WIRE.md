# Stage 10 — Wire / software I2C master

Experimental Arduino-style `Wire` master using **open-drain GPIO software I2C**.
This stage does **not** implement the native UC3B TWI peripheral. Host protocol
tests and target compilation are required; external I2C communication remains
**HARDWARE NOT TESTED** because no device/pull-ups were available.

## Backend choice and scope

The legacy TWI block automatically sends STOP when its transmit shifter and
holding register empty (32059L §19.14.1). Its internal-address facility supports
particular combined reads, not arbitrary Wire-style held-bus sequences. It also
has receive-overrun and reset-status errata (§31.2). A later native/DMA backend
needs separate validation of those restrictions and USB/timer concurrency.

This first master uses GPIO so that address-only probes, ACK/NACK reporting and
general repeated STARTs have their actual bus semantics. `endTransmission(false)`
sends the address/data immediately and checks ACK; it does not merely defer the
write until a later read. No fabricated successful probe, dummy data byte or
implicit STOP is substituted. USB/timer interrupts are not masked by this driver.

Only a single foreground master is supported. There is no slave/callback API,
DMA, ISR use, multi-master scheduling, native TWI offload, 10-bit addressing,
general call, SMBus-specific protocol, 400 kHz mode or arbitrary pin selection.
This is a bounded, synchronous Wire subset, not full Arduino Stream/Print ABI
compatibility or a claim that every sensor library works unchanged.

## Pins and electrical setup

| Name | Arduino logical index | MCU pad | Operation |
| --- | --- | --- | --- |
| SDA | 14 | PA10 | Open-drain GPIO |
| SCL | 15 | PA09 | Open-drain GPIO |

These append to the existing map; D0, A0–A7, LED_BUILTIN and all SPI indices are
unchanged. They are MCU pad names, not connector pin numbers. The route does not
overlap the fixed LED, crystal, ADC or current SPI bus. A custom D0 can alias a
bus pad; do not use GPIO/another peripheral on the same pins while Wire owns them.

External pull-ups to a supply compatible with the board's VDDIO and the target
device are required. This stage does not establish connector routing or 5 V
tolerance. `Wire.begin()` disables internal pull-ups and preloads the GPIO latch
LOW. A LOW bit enables the output driver; a HIGH bit disables it and samples the
actual pin. The driver never actively drives HIGH. `Wire.end()` releases both
pins as floating GPIO. No TWI, clock-tree, bootloader or interrupt-vector registers
are changed. The GPIO offsets and MCU pin identities are checked at compile time.

## API

```cpp
#include <Wire.h>
void setup() {
    Wire.begin();
    Wire.setClock(100000UL);
}
void loop() {
    Wire.beginTransmission(0x50); // Replace address/register for your device.
    Wire.write(0);                // Integer zero is a data byte, not a null pointer.
    uint8_t error = Wire.endTransmission(false);
    if (!error && Wire.requestFrom(0x50, 2) == 2) {
        int first = Wire.read();
        int second = Wire.read();
        (void)first; (void)second;
    } else {
        Wire.end(); Wire.begin();
    }
    delay(100);
}
```

Supported: `begin()`, `end()`, `setClock(hz)`, `beginTransmission(address)`,
`write(byte/integer)`, `write(buffer,count)`, `write(text)`,
`endTransmission(sendStop=true)`, `requestFrom(address,count,sendStop=true)`,
`available()`, `read()`, `peek()` and `flush()` (no-op: transfers are synchronous).
Only 7-bit unicast addresses `0x08..0x77` are accepted. A zero-byte transmission
is a real address-only write probe. Reserved addresses are rejected without
starting a transfer. No automatic scanner runs at initialization.

TX and RX buffers each hold 32 bytes. `write()` returns the number accepted;
TX overflow aborts the entire queued operation before it is sent. `requestFrom`
rejects counts above 32, returns the exact received count on success and zero on
failure. Failed/partial reads expose no stale or partial RX bytes. `read()` and
`peek()` return -1 when empty. A zero-length read is a no-op and does not release
a previously held bus. Calling `requestFrom` while assembling a transmission is
rejected. Clock/timeout settings cannot be changed inside that transmission or
while the HAL holds the bus.

Both write and read support `sendStop=false`. The bus remains owned with SCL
LOW until the next operation, which emits a repeated START. The final byte of
each read is NACKed, even when followed by repeated START instead of STOP.
Always follow a held transfer with another operation or `Wire.end()`; the
foreground driver cannot release an abandoned bus in the background. Calling
`begin()` finishes/releases an old held transaction and restores defaults.

The provided global `Wire` object needs no constructor or heap. `begin(address)`,
`onReceive/onRequest`, `Stream` helpers, Print formatting and the two-argument
AVR `setWireTimeout(timeout, resetFlag)` overload are not provided.

## Timing and failure behavior

`setClock()` accepts 1,000–100,000 Hz; default is 100,000. The requested value is
an **upper bound**, not a measured bus rate. CPU COUNT supplies a ceiling-rounded
half-period; GPIO/software overhead, interrupts, pull-up rise time and stretching
make the actual bus slower. The driver waits for SCL to actually rise before
starting the HIGH interval. It does not disable interrupts to enforce timing.
Physical timing, rise/fall times, noise rejection and electrical compliance have
not been measured. Firmware requires CPU clock metadata between 1 and 60 MHz;
nominal 115.2 kHz RCSYS is rejected. The reference board uses 12 MHz OSC0.

`setWireTimeout(us=25000)` accepts 1..1,000,000 us after `begin()`; zero cannot
disable timeouts. Each operation has one total CPU-count budget, including
stretching and software delays. Cycles/us round up on non-integral-MHz clocks.
A separate 100,000-poll cap per wait prevents hangs if COUNT stops and can abort
earlier than the requested timeout. Very slow transfers/large buffers may need
a larger budget. `getWireTimeoutFlag()` latches timeout status until
`clearWireTimeoutFlag()` or fresh `begin()`.

| Result | Meaning |
| --- | --- |
| 0 | Transfer completed and ACKed |
| 1 | TX buffer overflow; queued transmission not sent |
| 2 | Address NACK |
| 3 | Data-byte NACK |
| 4 | Invalid argument/state, busy SDA or line contention |
| 5 | Timeout / stopped counter / polling limit |

`endTransmission()` returns this result. `lastError()` supplies it for a failed
read or void configuration call. Boolean `Wire` means initialized with valid
settings and last error zero; it does not mean a device is connected.

After NACK the driver attempts STOP within the same deadline. Timeout or detected
contention releases both outputs; it never drives a HIGH or blindly generates
bus-clear clocks. A physically stuck device may still hold a line LOW. Fix the
wiring/target cause and call `end()/begin()` before retrying. After an API argument
error while a previous bus transfer was held, explicitly end that transaction.
Some bytes may already have reached the device before a transfer error: retry
according to that device's protocol. Contention detection is not multi-master
arbitration support.

## Examples and validation

- `WireScanner`: address-only scan with separate NACK versus bus-fault handling.
- `WireRegisterRead`: actual write + repeated START + two-byte read template.
- `WireApiSmoke`: initialization, invalid-argument and buffer checks only; it
  releases PA09/PA10 but performs no I2C START or device transaction. CDC and LED
  remain available. A PASS is an API/runtime check, not bus hardware validation.

Native tests run the actual bit engine with a scripted open-drain peer and inspect
clocked data, address direction, ACK/final NACK, START/STOP and held-bus behavior.
They also cover clock stretching, stuck lines, contention, COUNT wrap/freeze,
timeout recovery, API buffer limits, partial-read suppression and result codes.
Final target builds, API smoke evidence and regression commands are in
[TESTING](TESTING.md). No external I2C device, clock rate or signal waveform has
been hardware-validated; the owner confirmed no I2C wiring on 2026-10-31.

## References

- [UC3B datasheet 32059L](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32059.pdf):
  pin map, GPIO, §19.14.1 automatic STOP and §31.2 TWI errata.
- [NXP UM10204](https://www.nxp.com/docs/en/user-guide/UM10204.pdf):
  open-drain signaling, START/STOP, ACK/NACK, stretching and Standard-mode timing.
- [Arduino Wire public API](https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/Wire/src/Wire.h):
  API names/semantics only; no third-party implementation was copied.
