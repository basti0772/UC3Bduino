# USB CDC / SerialUSB (Stage 6)

The AT32UC3B0256 application implements one native full-speed CDC ACM function.
`SerialUSB` talks directly to the sketch; `Serial` remains the Stage 4 hardware
USART. **COMPILE PASS; STATIC REVIEW PASS; HARDWARE NOT TESTED.** Enumeration,
Windows COM binding and physical TX/RX have not been observed on a board.

## Hardware and clock

Hardware reference: [AT32UC3B complete datasheet 32059L](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32059.pdf),
chapters 4, 9, 12, 22 and 28. USBB has a transceiver, internal full-speed pull-up,
VBUS comparator, seven endpoints and 960 bytes of DPRAM. Full-speed is 12 Mbit/s;
there is no high-speed support. Device mode is forced with UIDE=0/UIMOD=1;
the application does not use the controller's host capabilities or ID/VBOF pins.

| AT32UC3B0256 TQFP64/QFN64 signal | Package pin |
| --- | ---: |
| DP / D+ | 50 |
| DM / D- | 51 |
| VBUS voltage monitor | 52 |
| VDDPLL | 53 |
| PA18 / XIN0 | 39 |
| PA19 / XOUT0 | 40 |

DP, DM and VBUS are dedicated pads, outside the GPIO/D0 mapping. Package pin
numbers are not Arduino pin numbers. VBUS's device-mode comparator threshold is
1.4 V. Verify the board connector, supplies, VBUS routing, oscillator, decoupling,
ESD protection and series resistors (datasheet table 28-25) before testing.

Registers start at `0xFFFE0000`; FIFO windows at `0xD0000000`. Each endpoint has
a 64 KiB virtual window, not additional RAM; byte accesses advance its FIFO in
hardware. Compile-time checks validate vendor structure offsets. Interrupt 544
maps to INTC group 17 and the new INT2 stub at EVBA+0x10c. Timer INT0 and UART
INT1 retain their existing stubs. USBB uses HSB mask bit 3, PBB bit 1 and GCLK3.

The existing clock HAL now owns USB PLL1/GCLK3. It accepts explicit OSC0 at
8, 12 or 16 MHz; RCSYS and other CPU oscillator frequencies leave USB disabled.
The physical reference must meet USB's ±0.25% tolerance. Nominal configuration
does not verify oscillator accuracy.

| OSC0 | PLL1 DIV | PLL1 MUL | VCO | PLL output / USB |
| ---: | ---: | ---: | ---: | ---: |
| 8 MHz | 0 | 5 | 96 MHz | 48 MHz |
| 12 MHz | 0 | 3 | 96 MHz | 48 MHz |
| 16 MHz | 0 | 2 | 96 MHz | 48 MHz |

DIV=0 gives FIN=OSC0 and FVCO=2×(MUL+1)×OSC0: FIN is within 4..16 MHz and
VCO within 80..240 MHz. PLLOPT=3 selects the low VCO range and divides output
by two. PLLCOUNT=63 qualifies lock; GCLK3 selects PLL1 without further division.
Readiness polls are bounded. Existing PLL1/GCLK3 ownership causes failure.
`end()` releases only USB's PLL/generic clock; peripheral bus clocks stay enabled.
USB enable/disable does not change MCCTRL, CKSEL, PLL0 or unrelated generic
clocks: CPU, HSB, PBA and PBB continue at the selected OSC0 frequency.

## VID/PID and installation

**USB VID/PID must be configured by the developer.** The repository has no
assigned identity. VID=PID=0 means unconfigured; `begin()` will not start the
controller or attach. No Arduino, Atmel, Microchip or other vendor identity is
provided for application CDC. The factory DFU identity remains exclusive to
the unchanged bootloader.

Use IDs you own or have explicit permission to use. The installer does not
confer that permission. There is no claimed public development VID/PID here.
The native host test's synthetic identity exists only inside a fake transport,
never in a target firmware build or on real USB hardware.

The central settings are in `src/usb/usb_config.h`. They can be overridden by
compiler flags. The installer accepts paired `-UsbVendorId` / `-UsbProductId`
(1..65535), requiring supported OSC0 settings with them. After assigning the
following variables from your actual allocation and verified board wiring:

```powershell
.\scripts\install-arduino-hardware.ps1 -SketchbookPath 'D:\ArduinoSketchbook' `
  -UsbVendorId $authorizedVid -UsbProductId $authorizedPid `
  -Osc0Hz $boardOsc0Hz -Osc0Mode $boardOsc0Mode -GpioPort $safePort -GpioPin $safePin
```

Configure toolchain, DFU tool and any physically verified legacy target as in
[DFU_UPLOAD](DFU_UPLOAD.md). Use a fresh destination; the installer refuses to
overwrite a package. It copies the canonical USB sources into the core's `usb/`
directory and records hashes. A sketch-local `#define` does not affect separately
compiled core files. Edit the central header before installation, or package
build flags, and rebuild without a stale Arduino cache.

Default manufacturer/product strings are `UC3Bduino` / `UC3Bduino CDC`. Each
string accepts at most 31 ASCII characters and is emitted as UTF-16LE. Serial
is empty (iSerial=0); supply a stable unique per-device value if available.
Changing identity/serial may change Windows' COM assignment. These strings do
not imply ownership of a USB vendor ID.

Power defaults to bus-powered, 100 mA, no remote wakeup. Set
`UC3BDUINO_USB_SELF_POWERED` and `UC3BDUINO_USB_MAX_POWER_MA` to match the board.
Suspend keeps clocks running to preserve the sketch. USB suspend-current
compliance is **not established**; board-specific low-power work and measurement
are required before claiming a bus-powered product complies with USB limits.

## Descriptors, endpoints and requests

One USB 2.0 device descriptor, one 67-byte configuration and two interfaces form
a single CDC function. Device class/subclass/protocol is 02/02/00, following
Microsoft's automatic usbser binding guidance. Interface 0 is CDC ACM 02/02/00
(no AT interpreter); interface 1 is data 0A/00/00. Functional descriptors are
CDC Header 1.20, ACM capability 0x02, Union linking 0/1, and Call Management with
capabilities zero. No IAD is needed for the single function. No alternate
settings, additional functions, device qualifier or other-speed configuration.

| Endpoint address | Type | Packet / banks | Purpose |
| --- | --- | --- | --- |
| 0x00 / 0x80 | Control | 64 / 1 | Requests and status |
| 0x81 | Interrupt IN | 16 / 1, interval 16 ms | SERIAL_STATE |
| 0x02 | Bulk OUT | 64 / 1 | PC → sketch |
| 0x83 | Bulk IN | 64 / 1 | Sketch → PC |

208 bytes of DPRAM are reserved, without DMA. EP0's hardware limit is one
64-byte bank; EP1..4 support 64-byte/two-bank configurations, EP5..6 up to 256.
Allocation proceeds in ascending order, deallocation descending; CFGOK is checked.

EP0 explicitly tracks setup/data/status. Responses are clamped to wLength,
including terminating ZLP where required; no-data requests use IN status.
New SETUP aborts the previous transfer, and OUT status wins over simultaneous
IN-ready. SET_ADDRESS applies after status ACK, using separate UADD/ADDEN writes.
Other validated changes take effect before status, so a following SETUP cannot
hide an ACK and lose accepted class settings.

Standard requests: GET_DESCRIPTOR (device/configuration/strings), SET_ADDRESS,
SET/GET_CONFIGURATION, GET_STATUS, endpoint SET/CLEAR_FEATURE(HALT), and
GET/SET_INTERFACE alternate zero. Unsupported or malformed requests stall.
Selecting a configuration, even the existing one, resets data endpoints and
software queues. Clear halt/SET_INTERFACE restores DATA0 and discards that
endpoint's in-flight bank, retaining software RX and unsent TX bytes.

CDC SET_LINE_CODING, GET_LINE_CODING and SET_CONTROL_LINE_STATE use interface 0.
Seven line-coding bytes are stored in USB byte order (AVR32 is big-endian);
default 115200 8N1. Stop/parity/data-bit encodings are validated. Baud is metadata
and does not change USB, CPU or UART speed. DTR/RTS are retained; SERIAL_STATE
reports virtual DSR/DCD following DTR. SEND_BREAK and communication-feature
requests are not advertised and stall. No UART bridge or 1200-baud reset.

## SerialUSB and buffers

`USBSerial SerialUSB` shares Stage 4's constructor-free `Print` callback,
decimal conversion and CRLF implementation. This core does not currently have
a `Stream` class; no second formatter or virtual ABI has been introduced.

| API | Behavior |
| --- | --- |
| `begin()` / `begin(115200)` | Initialize; compatibility baud argument ignored. Missing identity/clock leaves USB detached. Repeated begin reinitializes. |
| `operator bool()` | Configured, awake and host DTR asserted. False does not distinguish init failure from closed monitor. |
| `available()` | Number of bytes in RX ring. |
| `read()` / `peek()` | 0..255, or -1 when empty; peek preserves the byte. |
| `write(byte/buffer/string)` | Nonblocking count accepted; zero/short count when not ready or full. |
| `print()` / `println()` | Existing Print subset, including short return counts on backpressure. |
| `flush()` | Kick TX and wait up to 100 ms for ACKs/ZLP with the TC timebase; return when disconnected/suspended/halted; never clear RX. |
| `end()` | Detach, disable USB and its PLL/generic clock; clear USB queues/state. |

Call the API from foreground setup/loop with interrupts enabled. Begin/end and
waiting flush reject unsuitable context. There is one global transport, no heap
or runtime constructor. Static RX/TX arrays default to 128 bytes each (127 usable);
central size options accept powers of two from 128 to 1024.

RX admits whole packets. If space is insufficient, the driver holds the OUT bank,
masks its interrupt and lets hardware NAK further packets. Reading enough bytes
reenables the held interrupt. TX batches Print bytes into at most one 64-byte
packet per 1 ms SOF; a full final packet is followed by a ZLP after ACK. This
favors bounded ISR work over throughput: the scheduling ceiling is roughly
64 kB/s before short packets and host delays. Flush timeout preserves pending TX.

Reset, disconnect, end and configuration changes discard queued bytes/settings.
DTR close pauses new TX acceptance and transmission; already queued bytes remain
until reopening/reset. Suspend preserves queues and resumes on bus activity;
CPU/timebase keep running. No Arduino/Print calls occur in the ISR. Actual ISR
latency, stack high-water and sustained UART/USB coexistence are **TODO / VERIFY**.

## DFU → CDC, Windows 11 and Serial Monitor

1. Enter factory DFU through verified reset/ISP wiring; its USB device appears.
2. Upload with Stage 5's checked workflow. Launch requests reset/application start.
3. Application entry remains `0x80002000`; the bootloader's 8 KiB and SRAM key
   remain protected. Stage 6 changes no uploader behavior or bootloader bytes.
4. `SerialUSB.begin()` removes the pull-up and resets USBB, remaining detached
   for two full-speed frame periods (2 ms). This covers a host-driven packet/SOF
   plus USB 2.0's maximum 2.5 µs disconnect-detect interval. CPU COUNT measures it
   with interrupts enabled; this is not a Windows installation delay.
5. With VBUS present, CDC attaches under the developer's identity. The DFU device
   disappears and Windows enumerates CDC. Physical transition remains untested.

Microsoft documents [automatic usbser.sys installation for class 02/subclass 02](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-driver-installation-based-on-compatible-ids).
No custom or unsigned INF is supplied. Expected Windows 11 display is under
**Ports (COM & LPT)**, perhaps `USB Serial Device (COMx)` or `UC3Bduino CDC (COMx)`.
Name/number depend on Windows and device history; this is expected behavior,
not a tested COM-port result. Factory DFU uses a separate WinUSB binding; do
not replace the CDC binding with WinUSB.

Install/select UC3Bduino AT32UC3B0256, open USBSerialBlink, Verify, enter DFU,
Upload, wait for the CDC port, select it and open Arduino Serial Monitor. Baud
is metadata. Monitor normally asserts DTR; until then bool is false and these
examples print nothing. Blink never waits for USB, Hello's initial wait is at
most 3 seconds, and greetings repeat for late-opened monitors. Echo keeps the
next RX byte if TX is full. Test binary traffic with a host program too.

Uploading again requires manual DFU entry. Closing at 1200 baud does not reset
the board. `Serial` and `SerialUSB` remain independent objects.

## Debugging and hardware acceptance

| Symptom | Checks |
| --- | --- |
| Unknown USB Device | DP/DM, VBUS, supplies/ground, cable, oscillator/48 MHz reference, reset |
| Device Descriptor Request Failed | Clock accuracy/PLL lock, EP0 setup/status, IRQ vector, transition from DFU; capture USB trace |
| No COM port | Nonzero authorized IDs, 02/02 device class, complete configuration/CDC descriptors, usbser binding |
| Enumeration repeatedly restarts | Power/VBUS bounce, watchdog/reset, clock, descriptor/EP0 errors |
| DFU remains active | App reset address/image, released ISP input, app validity, Stage 5 launch log |
| CDC appears but no data | Correct COM port, DTR, begin/print calls, TX short counts, suspend/halt |
| `available()` always zero | Host sending to data OUT, correct COM/current firmware, IRQ enabled; opening alone sends no payload |
| GPIO/timing fails under traffic | Measure TC's 10 ms service budget/ISR latency; D0/OSC0/UART conflicts, power |

Optional UART diagnostics belong in foreground, e.g. logging SerialUSB bool
transitions through a separately configured Serial. No enabled production debug
output or ISR formatting. Record board/revision, oscillator, authorized identity,
descriptors, driver version and captures. Check resets during EP0 phases,
configuration/halts, 0/1/63/64/65/127/128+ bytes, duplex/backpressure, flush timeout,
disconnect/reconnect, suspend/resume, DFU transition, Serial Monitor, UART and
measured 1 Hz GPIO/millis behavior. See [TESTING](TESTING.md) for the distinction
between portable host tests and hardware evidence. No certification claim.

## References and provenance

- [USB 2.0 specification](https://www.usb.org/document-library/usb-20-specification): ch. 8/9 transfers/requests; 7.1.7.3 and table 7-13 disconnect.
- [CDC 1.2 / PSTN 1.2 package](https://www.usb.org/sites/default/files/CDC1.2_WMC1.1_012011_0.zip): descriptors, ACM requests and notifications.
- [AVR4907](https://ww1.microchip.com/downloads/en/appnotes/doc8447.pdf) and [AVR4920](https://ww1.microchip.com/downloads/en/AppNotes/doc8410.pdf): vendor CDC/device-stack context.
- ASF references were license-reviewed, not copied. This stack is original MIT
  code; inspected files/licenses are listed in [THIRD_PARTY](THIRD_PARTY.md).

No Host, HID, MSC, MIDI, audio, composite, runtime DFU or custom bootloader.
Stage 7 is packaging/easier installation. Performance, low-power and wider
Stream compatibility should follow measured hardware tests.
