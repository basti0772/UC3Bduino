# Stage 4 – HardwareSerial / UART

`Serial` is one hardware USART with polling TX, interrupt RX and decimal Print.
The eight example sketches compile with AVR32 GCC/G++ 4.3.2. Hardware execution,
oscillator startup and electrical/baud measurements are **NOT TESTED**.

## Select the actual board wiring first

The repository identifies the MCU, not a board schematic. The default route is
**unconfigured** (255): `Serial.begin()` leaves Serial inactive and writes return
zero. No default TX/RX pads or crystal frequency are assumed. The installer
accepts these MCU routes, checked against datasheet table 4-1 and the installed
`uc3b0256.h`. Pad names below are not connector positions:

| Installer `-SerialRoute` | HAL route | Instance | TX pad | RX pad | Function |
| --- | ---: | --- | --- | --- | --- |
| U0A | 0 | USART0 | PA19 | PA18 | A (0) |
| U0C | 1 | USART0 | PB11 | PB10 | C (2) |
| U1A | 2 | USART1 | PA23 | PA24 | A (0) |
| U1C | 3 | USART1 | PB02 | PB03 | C (2) |
| U2B | 4 | USART2 | PA26 | PA27 | B (1) |
| U2C | 5 | USART2 | PA21 | PA20 | C (2) |

Only the selected instance is initialized; there are no Serial1/Serial2 objects.
**U0A conflicts with OSC0**: PA18 is XIN0, PA19 is XOUT0 in crystal mode.
The HAL and installer reject U0A with OSC0. The installer also rejects D0
overlapping selected serial or oscillator pads. Check package availability,
Nexus trace ownership, attached circuitry and accessible connectors yourself.
Manual edits to variant files must preserve these ownership rules.

115200 baud needs a faster source than the default nominal 115.2 kHz RCSYS.
Stage 4 adds an explicit, reset-time OSC0 option, 0.4..16 MHz, with no PLL.
The board's actual crystal or external-clock frequency must be supplied; see
[CLOCK](CLOCK.md). This is the **compile-only fixture used in validation**, not
a recommendation to flash an unidentified board:

```powershell
.\scripts\install-arduino-hardware.ps1 -SketchbookPath 'D:\UC3B-serial-test' -ToolchainPath 'D:\Tools\avr32-toolchain' -SerialRoute U0C -Osc0Hz 12000000 -Osc0Mode Crystal -GpioPort A -GpioPin 3
```

Replace all paths and board parameters with verified values. `-Osc0Mode External`
means a driven clock on XIN0; `Crystal` means a crystal on XIN0/XOUT0. The
installer generates `boards.local.txt` for F_CPU/clock/buffer defines and
`variants/uc3b0256/serial_config.h` for the route. With no oscillator options,
the existing RC behavior is retained. Install to a fresh sketchbook package;
the installer refuses to overwrite an existing one.

## Clock and baud calculation

USART uses its PBA module clock (`MR.USCLKS=0`), asynchronous normal mode,
8 data bits, no parity, one stop bit. `MR.OVER=1` selects 8x oversampling;
`BRGR.FP=0` leaves the fractional divider disabled. The 16-bit CD field is:

```text
CD = round(PBA_Hz / (8 * requested_baud))
actual_baud = PBA_Hz / (8 * CD)
error_percent = 100 * (actual_baud / requested_baud - 1)
```

The HAL calculates this from `uc3b_clock_get_pba_hz()` using bounded 32-bit
integer arithmetic. It rejects zero baud, baud above PBA/8, CD outside 1..65535,
PBA outside 1..60 MHz, or nominal absolute error above **2%**. It does not
silently claim that an unsupported rate was configured. The 60 MHz arithmetic
bound is the MCU ceiling, not a new clock mode; this core enables OSC0 only up
to 16 MHz. No hardcoded baud table, fractional synthesis or software divider is
used. Clock tolerance adds to the nominal divisor error.

At **12,000,000 Hz nominal PBA** (the compile fixture):

| Requested baud | CD | Actual baud, rounded for display | Nominal error |
| ---: | ---: | ---: | ---: |
| 9600 | 156 | 9615.384615 | +0.160256% |
| 19200 | 78 | 19230.769231 | +0.160256% |
| 38400 | 39 | 38461.538462 | +0.160256% |
| 57600 | 26 | 57692.307692 | +0.160256% |
| 115200 | 13 | 115384.615385 | +0.160256% |

These are calculated ratios, not measured or exact decimal values. The baud
test uses rational arithmetic for the error bound. At nominal RCSYS the maximum
hardware rate is only 14400 baud; all five requested rates above are rejected
by this implementation (9600 would round to CD=2, 7200 baud, a -25% error).
Use an identified stable source for PC serial testing. Other OSC0 frequencies
can reject particular rates; for example, integer division at 16 MHz rejects
115200 (+2.124% at CD=17). This minimal driver does not promise every rate at
every supported source frequency.

## Arduino API

```cpp
void setup() {
    Serial.begin(115200);          // same as begin(115200, SERIAL_8N1)
    if (!Serial) { return; }       // invalid route/rate/format => inactive
    Serial.println("UC3Bduino");
}
void loop() {
    if (Serial) {
        Serial.println(millis());
        delay(1000);
    }
}
```

Use the global `Serial` object. `operator bool()` reports configuration success,
not whether a PC is connected. There is no USB-enumeration wait. An invalid
second `begin()` ends the old connection, clears RX and leaves Serial inactive.

| Operation | Contract |
| --- | --- |
| `begin(baud[, SERIAL_8N1])` | Drain old TX, reset old RX, configure selected USART; other frame constants rejected |
| `available()` | Unread buffered byte count, 0..63 by default |
| `read()` | Next byte 0..255, or -1 if empty |
| `peek()` | Same without consuming the byte |
| `write(uint8_t)` | Wait for TXRDY, write THR; return 1, or 0 when inactive |
| `write(const uint8_t *, size_t)` | Binary bytes, including NUL; stop on failed write |
| `write(const char *)`, `print(const char *)` | NUL-terminated text; null pointer writes nothing |
| `print(char/int/unsigned int/long/unsigned long)` | Character or decimal integer; includes LONG_MIN/ULONG_MAX |
| `println()` and the above value overloads | Append CRLF; `println(millis())` supported |
| `flush()` | Wait for CSR.TXEMPTY (holding register **and shift register** empty), preserving RX |
| `end()` | Drain TX, disable peripheral interrupts, reset/disable RX/TX, clear ring and release TX/RX as floating GPIO inputs |

Print methods return the number of bytes accepted. Output remains blocking,
with interrupts enabled so RX and the timer can run. There is no TX buffer or
timeout; a stopped/reconfigured peripheral clock can stall TX waits. After
`end()`, clocks remain enabled and the selected INTC entry remains installed
with its peripheral sources disabled. Previous GPIO modes/pull-ups are not
restored; other pads are untouched.

`Print` is a small callback-based base class, avoiding vtables and global
constructors in this startup runtime. The provided Serial object's callback is
zero-initialized in BSS and assigned by begin. Custom derived sinks must assign
their callback before use. This is **not the full virtual Arduino Print/Stream
ABI**. No Stream hierarchy, float/string objects, format bases, parsing, timeouts,
heap, sprintf, STL, exceptions or RTTI are provided. Creating extra HardwareSerial
objects is unsupported; they would share the same single HAL instance.

## RX ring, interrupts and errors

`SERIAL_RX_BUFFER_SIZE` defaults to 64, with **63 usable bytes**. One slot stays
empty to distinguish full from empty. Set `-SerialRxBufferSize` at installation
to a power of two in 2..256; it applies consistently to all core files. New
bytes are discarded when full, preserving the order of existing unread bytes.
There is no flow control or overflow counter in this stage.

The ISR reads CSR, drains RHR on RXRDY, and discards the byte if OVRE, FRAME or
PARE was reported. It resets sticky errors with CR.RSTSTA, including errors
without RXRDY. Reading RHR clears RXRDY. Overrun can mean lost bytes; subsequent
recovery does not recover those bytes. No printing, waiting, allocation, timer
calls, division or other HAL calls occur in the ISR.

RX storage and aligned 32-bit indices are static and volatile. Foreground
`available/read/peek` use short GM-masked sections preserving the old mask;
they cannot observe an ISR halfway through publishing a byte. Begin/end also
serialize ownership changes. Public APIs are foreground-only and not reentrant;
do not use them in ISRs, NMI or exception handlers. All clock changes are
reset-time only. UART does not modify the timer's state or interrupt level.

USART RX uses **INT1**, above the timer's INT0, through a new branch stub at
EVBA offset **0x108**. The linker checks EVBA OR offset. The selected USART's
whole INTC group is reserved to this driver; other sources must not be enabled
there without a dispatcher. The ISR uses GCC's interrupt attribute and `rete`.
IRQ clear/disable is followed by a CSR read and three NOPs for deassertion to
reach INTC. The common GM helper retains the two-NOP Rev B masking workaround.
Timer init remains the single global interrupt-enable point; Serial clears I1M
after configuration while preserving GM.

At roughly 115200 8N1, one frame takes about 86.8 us, and 63 queued bytes take
about 5.47 ms. Keep interrupts responsive enough to drain the hardware receive
register before the next frame, and drain the software buffer often enough.
The 1-second delay in SerialMillis is for output only; incoming bursts can
overflow during that delay. No sustained-rate/latency claim is hardware-tested.

## Verified register references

Primary source: [AT32UC3B datasheet 32059L](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32059.pdf),
tables 4-1/4-4, memory map, PM clock table, INTC grouping, sections 21.5,
21.6.2.1–21.6.2.3 and 21.7.1–21.7.9. Interrupt masking/acknowledgment follows
[AVR32 UC TRM 32002F](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32002.pdf),
sections 3.7/3.8 and the datasheet Rev B errata. USART errata involving Manchester,
RXBREAK, handshaking, synchronous/SPI modes and DMA concern features not enabled
here. Actual silicon revision still requires verification.

| Instance | Base | PBA mask bit | Header IRQ | INTC group / line |
| --- | --- | ---: | ---: | --- |
| USART0 | 0xFFFF1400 | 7 | 160 | 5 / 0 |
| USART1 | 0xFFFF1800 | 8 | 192 | 6 / 0 |
| USART2 | 0xFFFF1C00 | 9 | 224 | 7 / 0 |

| Register / field | Offset or bit |
| --- | --- |
| CR, MR, IER, IDR | 0x00, 0x04, 0x08, 0x0C |
| CSR, RHR, THR, BRGR | 0x14, 0x18, 0x1C, 0x20 |
| CSR RXRDY / TXRDY / TXEMPTY | 0 / 1 / 9 |
| CSR OVRE / FRAME / PARE | 5 / 6 / 7 |
| CR RSTRX / RSTTX / RSTSTA | 2 / 3 / 8 |
| CR RXEN / RXDIS / TXEN / TXDIS | 4 / 5 / 6 / 7 |
| BRGR CD / FP | bits 15:0 / 18:16 |

Installed official headers supply all definitions; C99 layout assertions check
used addresses and offsets. No vendor driver source is copied. UART delegates
pad selection/release to the extended GPIO HAL; it does not duplicate GPIO MMIO.

## PC test procedure — TODO / VERIFY

1. Identify board, package, silicon revision, oscillator frequency/mode, VDDIO
   and accessible TX/RX pads from the schematic. Confirm that no other circuit
   drives the chosen pins; configure the package accordingly.
2. Use a USB-UART adapter whose logic voltage matches the board's actual VDDIO.
   The MCU's specified VDDIO range is 3.0..3.6 V (datasheet table 28-1).
   Do not assume 5 V tolerance or use RS-232 voltage levels. If VDDIO is 3.3 V,
   use compatible 3.3 V UART levels. Do not connect adapter power unless the
   board's power arrangement has been separately verified.
3. Wire **MCU TX → adapter RX**, **MCU RX ← adapter TX**, and **GND ↔ GND**.
   Use the pads selected above; the repo does not know the connector numbers.
4. Build SerialHello and program/reset using an already verified external
   method. This package has no automatic uploader. Open the adapter's PC port
   at 115200, 8N1, no hardware flow control. Reset after opening to catch the
   one-time `UC3Bduino` line. `Serial` is not native USB/CDC.
5. Measure TX idle/high level and bit time with a scope/logic analyzer. For the
   12 MHz/CD=13 fixture, expected nominal bit time is 8.666667 us; compare the
   measurement and oscillator tolerance, not just legible terminal text.
6. Run SerialMillis: expect increasing decimal milliseconds at roughly 1-second
   intervals plus transmission/loop overhead. Confirm millis/micros and a GPIO
   pulse still behave while RX traffic is present.
7. Run SerialEcho with local echo disabled. Send single characters, NUL/0xff
   with a binary terminal and bursts; compare received bytes. Repeat at 9600,
   19200, 38400 and 57600 after changing both sketch and terminal baud.
8. For overflow, temporarily pause foreground RX reads, send more than 63 bytes,
   then drain and verify the oldest 63 remain. Test mismatched frame/baud and
   recovery after restoring 8N1. Confirm flush preserves pending RX and end
   disables reception; repeated begin must start with an empty buffer.

Record real results in [TESTING](TESTING.md). No MCU, adapter, scope or simulator
was available for these checks. CLI success is not a hardware pass.
