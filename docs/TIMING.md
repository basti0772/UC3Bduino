# Stage 3 – Arduino timing foundation

The Arduino core reserves **TC channel 0** for system timing: **Reserved by
UC3Bduino core for system timing**. Its interrupt group (INTC group 14) is also
reserved until a dispatcher exists; channels 1/2 must not enable interrupts into
that group. PWM, tone, Servo and user libraries must not reconfigure TC0, its
clock, RC, interrupt mask or block synchronization.

## Timer choice and verified hardware

The AT32UC3B0256 has three 16-bit TC channels, an RTC and CPU COUNT/COMPARE.
TC0 uses TIMER_CLOCK3 (PBA/8), which has the same division on Rev B and later
silicon. COUNT/COMPARE reset behavior differs on Rev B, and RTC is unsuitable
for this finer-resolution timebase. No external crystal is assumed.

The installed `tc_222.h`, `intc_101.h` and `core_sc0_130.h` supply register
definitions. Their hashes already appear in [DEVICE_HEADERS](DEVICE_HEADERS.md).
Compile-time checks verify TC address/stride/offsets, IRQ0=448 (group 14, line 0),
INTC registers and clock indices. Capture mode with CPCTRG resets CV on RC;
CPCS is read-to-clear in SR. No TC output pin is selected.

Primary references: [AT32UC3B datasheet 32059L](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32059.pdf),
chapters 12/23, table 23-5 and Rev B errata pp. 668, 671–672;
[AVR32 UC TRM 32002F](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32002.pdf),
sections 2.6, 3.7 and 3.8. The installed GCC manual documents the AVR32
`interrupt` attribute. Actual disassembly confirms `rete` and native `divu`.

## Configuration and resolution

| Property | Current configuration |
| --- | --- |
| Source | `uc3b_clock_get_pba_hz()`, nominal 115200 Hz |
| Prescaler | 8 |
| Counter frequency | nominal 14400 Hz |
| RC | 144 ticks |
| Interrupt rate | nominal 100 Hz (10 ms) |
| Priority | INT0, lowest; no nested higher-priority users in Stage 3 |
| millis return granularity | integer milliseconds; interpolated from CV |
| micros counter resolution | nominal 69.444… us, returned as integer 69/70 us steps |

The 100 Hz rate limits ISR overhead on the slow CPU. It does not limit reads to
10 ms resolution: snapshots include CV. Conversion uses exact integer division,
with no cumulative rounding drift and no 64-bit production arithmetic. The
current ISR only acknowledges status and adds 10 ms / 10000 us to two unsigned
counters. Division occurs at initialization or after snapshot interrupts are
restored, never in the ISR.

Resolution is not accuracy or API latency. [RCSYS](CLOCK.md) is nominal, with
substantial voltage/temperature/part variation. GPIO, function calls, polling and
interrupt service add overhead at 115.2 kHz. No 1-us pulse precision or measured
ISR cycle budget is claimed. Debug stops and sleep are not elapsed wall time.

Initialization requires PBA to be a multiple of 800 Hz and RC to be 10..65535.
It rejects unsupported clocks rather than silently rounding. A future faster
clock must be configured before timer init, reported correctly by the clock HAL,
and satisfy those bounds. Live clock changes are unsupported. For example,
48 MHz fits this calculation; undivided 60 MHz does not. Neither clock is enabled
by Stage 3. Changing CPU/PBA division also requires a fresh interrupt-latency
review; this stage uses CPU=HSB=PBA=PBB.

## Startup, vector and interrupt ownership

The existing reset code still initializes SP/EVBA/data/bss with GM set.
Arduino init calls clock init, then timer init, then setup. Timer init validates
the clock and masked context, prepares counters and TC/INTC, starts TC and
clears I0M/GM at one documented point. A repeated/invalid init fails and Arduino
main halts rather than entering setup with invalid time. Bare-metal main does
not initialize the timer or enable interrupts.

A new branch stub at EVBA offset 0x104 targets the timer ISR. The linker checks
its address and the CPU's EVBA-OR-offset rule; 512-byte EVBA alignment suffices
for this fixed offset. The weak default traps when the timer is absent. The C
ISR uses GCC's AVR32 interrupt attribute and hardware stack entry/exit; there is
no generic interrupt framework. Fixed exception traps remain unchanged.

## Snapshot races and wraparound

Both ISR and reader share volatile 32-bit millisecond/microsecond epochs. The
reader saves GM, masks interrupts with the Rev B two-NOP workaround, reads CV,
then reads SR. If CPCS is set, it advances both epochs once and rereads CV.
Otherwise it keeps the original CV, a valid sample even if compare occurs just
after SR. It restores the caller's GM state; it does not enable previously
masked interrupts. The ISR uses the same acknowledge-and-advance operation,
so a period serviced by a reader is not counted twice.

Only these paths may read TC0 SR, because a read clears the pending flag.
A non-clearing CV read and propagation NOPs separate acknowledgment from
unmasking/return. GM (or hardware I0M in the ISR) prevents unsafe withdrawal of
an interrupt while the CPU accepts it. Higher-priority ISRs and NMI must not call
timing functions or touch their state in this stage.

**The entire unserviced interval, including snapshot/ISR work, must be shorter
than 10 ms.** A one-bit status flag cannot count two missed periods. Long global
interrupt masking or a long higher-priority ISR loses elapsed periods. Frequent
reads can service one pending period while GM is set; this is not permission
to mask interrupts arbitrarily long. On-device latency measurement is pending.

`millis()` wraps modulo 2^32 after about 49.7 days, and `micros()` after about
71.6 minutes. Independent epochs preserve both wrap periods; micros is not
computed from the wrapped millis value. Unsigned elapsed subtraction handles
ordinary intervals across either boundary. Time starts when TC is triggered
inside init, excluding earlier reset/clock setup.

## Waiting and scope tests

`delay()` polls the millisecond clock. `delayMicroseconds()` polls the same
hardware-backed microsecond clock, avoiding an unverified cycle-count loop.
Both preserve interrupt state and return immediately for zero. Bounded chunks
(1000 ms / 10000 us) avoid a missed termination window for UINT32_MAX requests;
chunk/call overhead can extend long waits. Quantization is below one timer tick,
but total call latency and interrupt jitter must be measured separately.

Select a safe real pad for logical D0 using the installer. No LED is assumed.
Expected nominal observations, with RC error and software overhead included:

| Sketch | Measurement target |
| --- | --- |
| Blink | HIGH about 500 ms, LOW about 500 ms, period about 1 s |
| MillisTest | HIGH about 250 ms, LOW about 750 ms, without blocking delay |
| MicrosPulse | Requested HIGH wait 10000 us, LOW wait 90000 us; inspect `lastHighElapsedUs` |

MicrosPulse deliberately uses longer waits for the slow RC baseline. Changing
its constants to 100/900 us tests short-wait overhead, not a guaranteed 100/900-us
waveform. GPIO and read/write overhead can dominate such pulses. Measure an empty
GPIO pulse and repeated micros calls first, then log requested/measured widths,
oscillator frequency, jitter and debugger state. No hardware measurements or
upload were performed; see [TESTING](TESTING.md).
