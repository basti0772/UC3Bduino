# ADC / analogRead

Stage 8, AT32UC3B0256 only. `int analogRead(uint32_t pin)` returns the native
**10-bit value 0..1023**, or **-1** on an invalid/non-ADC pin, GPIO selection
failure, busy ADC, unsupported clock or timeout. Zero remains a valid reading.
Pass an Arduino pin (`A0`), not a hardware channel number. Call from foreground
`setup()`/`loop()`; ISR/concurrent use and direct register access are unsupported.
Build and host logic checks do not establish physical ADC performance; see
[TESTING](TESTING.md). **HARDWARE NOT TESTED.**

## Hardware and sources

The original HAL uses the installed Atmel `uc3b0256.h` / `adc_200.h` declarations,
with compile-time register-layout and pad assertions. No ASF or vendor driver
source is imported. Header provenance/licenses are in [DEVICE_HEADERS](DEVICE_HEADERS.md).
Primary reference: [Microchip/Atmel AT32UC3B complete datasheet, 32059L–01/2012](https://ww1.microchip.com/downloads/en/DeviceDoc/doc32059.pdf),
table 4-1 (pin multiplexing), section 5.6.3 and 28.4 (reference), chapter 25
(ADC), tables 28-20..24 (electrical/timing/accuracy) and the silicon errata.

The chip has eight multiplexed SAR inputs. It also offers an 8-bit mode; this
core leaves LOWRES clear for 10 bits. Software START converts the enabled
channels and then waits for another trigger. Hardware triggers from timers or
the external ADC trigger are available but disabled (TRGEN=0). This revision
has no independent free-running MR bit. ADC interrupt sources include channel
EOC/overrun and DRDY/general overrun; the HAL disables all ADC interrupts and
uses EOC polling, without INTC/vector, DMA, automatic scan or background work.

## Reference, supply and input limits

**ADVREF is an externally supplied reference input, not a software-selectable
internal reference or a power output.** On the 64-pin package ADVREF is pin 18,
GNDANA pin 17 and VDDANA pin 19. Verify the actual board routing, reference
source, analog supply and ground before use. An unused ADC's ADVREF may have
been tied to ground on a board; software cannot repair that configuration.

Table 28-21 specifies **2.6 V <= ADVREF <= VDDANA**. Table 28-4 additionally
limits ADVREF to **3.6 V**. Meet both constraints and all supply specifications.
The valid conversion input range is **0 V through ADVREF**, measured against
analog ground. Never drive an ADC input outside that range; do not connect a
5 V signal. Digital-pin tolerance is not an ADC operating specification.
Keep inputs within the powered device's limits during power sequencing too.

Section 5.6.3 requires external reference decoupling. Table 28-5 lists typical
**10 nF and 1 uF NPO** capacitors; follow the board layout/schematic and reference
source's stability/load requirements. Reference startup/stability is a board
responsibility, separate from the ADC cell startup delay below. There is no
`analogReference()` or `analogReadResolution()` API in this stage. No voltage
example assumes 3.3 V or 5 V; raw values need the measured reference and an
appropriate error budget before conversion to voltage.

## One pin table

`Uc3bPinDescription` extends the existing variant map with `adc_channel` (-1 for
non-ADC pads) and `adc_function`. The digital API still uses its port/pin fields.
The following MCU assignment is fixed; board connector/LED names are unknown.

| Arduino name | Arduino number | MCU pad | ADC channel | Peripheral | 64-pin package pin |
| --- | ---: | --- | ---: | --- | ---: |
| A0 | 1 | PA03 | 0 | A (0) | 9 |
| A1 | 2 | PA04 | 1 | A (0) | 10 |
| A2 | 3 | PA05 | 2 | B (1) | 11 |
| A3 | 4 | PA06 | 3 | B (1) | 12 |
| A4 | 5 | PA07 | 4 | B (1) | 13 |
| A5 | 6 | PA08 | 5 | B (1) | 14 |
| A6 | 7 | PA30 | 6 | A (0) | 15 |
| A7 | 8 | PA31 | 7 | A (0) | 16 |

`NUM_ANALOG_INPUTS=8`, `NUM_DIGITAL_PINS=9` (D0 plus the eight A-pins, which can
also be GPIO). D0 remains Arduino number 0, with installer-configured routing or
an invalid default. If configured on an ADC pad, D0 aliases that A-pin and
`analogRead(D0)` works. For example, **D0=PA03 and A0 are the same wire**: do not
run Blink on it while connecting an analog source. D0 on PB03 has no ADC;
`analogRead(D0)` then returns -1 without touching GPIO or ADC. `analogRead(0)`
means D0, not channel 0. Out-of-range Arduino numbers also cause no MMIO access.

`analogRead()` uses `uc3b_gpio_select_peripheral`, releasing only the selected
pad's GPIO output/pull-up/interrupt and setting its verified peripheral mux.
It leaves that pad in analog peripheral ownership afterward. No preceding
`pinMode()` is required; use `pinMode()` explicitly to regain GPIO ownership.
The UC3B GPIO register set exposes no separate digital-input-buffer disable
control to add here; no AVR-style DIDR or invented register bit is used.
Serialize pad ownership changes. PA03/PA04 can also serve USB OTG ID/VBOF, but
the current USB **device** implementation uses neither of those mux functions.
The A-pins do not overlap OSC0 or the supported UART TX/RX routes. Board-specific
loads and alternate functions must still be checked.

## Clock and sampling times

Lazy initialization occurs on the first valid conversion, then is reused.
ADC owns its PBA clock mask bit (CLK_PBA index 68, PBAMASK bit 4). Enabling it
preserves all other bits and waits for MSKRDY with a bound. CPU, bus dividers,
TC0, USB PLL1/GCLK3 and UART clock configuration are unchanged.

From `uc3b_clock_get_pba_hz()`:

```text
divider = ceil(PBA_Hz / 5,000,000)
PRESCAL = divider - 1
ADCClock = PBA_Hz / (2 * divider)       <= 2.5 MHz
STARTUP = 15  -> 128 ADC clocks        >= 51.2 us
SHTIM   = 15  -> 18 ADC clocks         >= 7.2 us
conversion after tracking = 10 clocks
```

The 10-bit electrical limit is **5 MHz**. The conservative 2.5 MHz target leaves
clock margin; the field values exceed the **20 us cell startup** and **600 ns
minimum acquisition** requirements. MR values use the datasheet's `(STARTUP+1)*8`
and `(SHTIM+3)` formulas. The HAL also waits at least 128 ADC periods using CPU
COUNT after initial configuration, before START, in normal mode (SLEEP=0).
This avoids relying on the sleep sequencer for the first normal-mode sample.

| Nominal PBA | PRESCAL | ADC clock | First initialization wait | Track + conversion |
| ---: | ---: | ---: | ---: | ---: |
| 115200 Hz RCSYS | 0 | 57600 Hz | 2222.3 us | 486.2 us |
| 8 MHz OSC0 | 1 | 2 MHz | 64 us | 14 us |
| 12 MHz OSC0 | 2 | 2 MHz | 64 us | 14 us |
| 16 MHz OSC0 | 3 | 2 MHz | 64 us | 14 us |

Values are rounded theoretical hardware durations, excluding software, bus
synchronization and interrupt overhead, not measured sampling rates. The HAL
accepts nominal PBA >= 100 kHz and PBA <= CPU <= 60 MHz; current clock selections
fit this range. Its CPU/PBA startup ratio rounds upward for divided buses.
Clocks must stay under the clock HAL's ownership; live clock changes are not
supported. RC tolerances affect timing. PRESCAL never exceeds 11 in these
bounds, inside both the chapter's /128 prose limit and the wider MR field.

## Conversion and failure recovery

1. Validate Arduino pin metadata, then select its peripheral function.
2. Acquire the ADC's short busy guard; initialize if needed (software reset,
   interrupts disabled, channels off, MR configured, startup delay).
3. With ADC idle, disable channels, read the selected CDR to clear old EOC,
   and read LCDR to clear old DRDY. Enable **only** the selected channel.
4. Write CR.START and poll that channel's SR.EOC. On success read CDRn
   (which clears EOC), read LCDR, disable channels and return the 10-bit value.
5. On timeout reset the ADC **before** changing channel enables; mark it
   uninitialized so the next call retries initialization. Return -1.

MSKRDY and EOC waits have a nominal **10 ms CPU COUNT deadline** and a
**100000-poll cap**. The startup wait has the same poll cap in case COUNT stops.
Elapsed subtraction tolerates COUNT wrap. Interrupts retain the caller's prior
mask while waiting; only busy-guard and PM mask updates briefly mask GM. Long
interrupt service can extend wall time; this is not a real-time deadline or a
watchdog. Normal operation does not require timer IRQs to enforce the timeout.
The HAL output pointer stays unchanged on failure. No floating point, heap or
runtime library is used. Only one foreground ADC owner is supported; the busy
guard rejects nested HAL calls but does not make GPIO ownership thread-safe.

The Rev B multi-channel erratum is avoided by enabling one channel per trigger.
No completion decision depends on overrun flags (which have a Rev B clearing
erratum). Normal mode avoids the sleep-mode erratum. Chapter 25 does not require
discarding a first sample after valid startup/tracking, so no dummy conversion
is invented. Changing channels during an active conversion is explicitly
forbidden by section 25.6.4; the timeout-reset ordering honors this. External
reference settling and a high-impedance signal still require board validation.

## Source impedance and accuracy

Section 25.6.7 relates acquisition time to source impedance; there is no input
buffer amplifier. No numeric guaranteed maximum source resistance is specified
in that section or the ADC electrical tables. Table 28-20 gives typical internal
tracking resistance **350 ohm** and sample/hold capacitance **12 pF**; table
28-22 gives typical input capacitance **7 pF** and maximum leakage **1 uA**.
The longer tracking time helps settling, but is not a guarantee for arbitrary
sensors, multiplexers, filters or high-value potentiometers. Use a low-impedance
source/buffer and verify settling when alternating low and high inputs. Leakage
also causes a DC error proportional to source resistance, regardless of track time.

At the datasheet's stated 5 MHz test condition, 10-bit table 28-24 lists maximum
absolute accuracy error 3 LSB, INL 2 LSB, DNL 2 LSB and offset/gain errors up to
2 LSB; DNL at 2.5 MHz is specified up to 1 LSB. These are device specifications
under their stated conditions, **not a calibrated board accuracy promise**.
Reference tolerance/noise, offset, gain, INL/DNL, source impedance, supply noise,
temperature and PCB layout affect readings. Do not sum these as independent
guarantees or equate 10-bit output width to 10-bit absolute accuracy.

## Examples and pending hardware test

`AnalogRead` prints A0 every 100 ms; `AnalogMultiChannel` prints successive A0
and A1 conversions. Both default to SerialUSB, never wait for a host, and can
use **HardwareSerial by setting `ADC_USE_USB` to 0** near the top of the sketch.
Configure the physical UART route and a suitable oscillator as in [SERIAL](SERIAL.md)
for 115200 baud; default RCSYS cannot supply that baud. For SerialUSB configure
verified 8/12/16 MHz OSC0 and authorized VID/PID as in [USB_CDC](USB_CDC.md).
USB hardware operation remains untested. The UART alternative is compile-tested,
also awaiting physical validation. A missing USB connection does not stop ADC
sampling but can discard printed output under the existing USB buffering policy.

For a board test, first inspect the schematic, measure the analog supply and
ADVREF against GNDANA, and ensure the reference can drive the chosen load. Use
a low-value potentiometer (for example 1 kohm if its reference source supports
the added load), with ends at the verified reference and analog ground, wiper
at A0/PA03. Alternatively use a suitable buffered adjustable source within
0..ADVREF. Do not load a weak precision reference with a potentiometer directly.

```text
verified ADVREF --- potentiometer --- GNDANA
                         |
                      A0 / PA03
```

Expect near 0 at ground, roughly 512 at half reference, and near 1023 at
reference, with error/noise as above. Record actual DMM voltages, reference,
silicon revision, firmware/clock configuration and multiple raw readings.
For channel switching connect A0 near ground and A1 near reference, alternate
reads, then swap levels. Check for carryover, first-read effects and -1 errors.
Repeat with UART and USB output plus millis/micros/delay; use a **different,
unloaded** GPIO for any waveform test. Confirm other GPIO/UART/USB functions
continue operating. These measurements, latency, reference startup and source
impedance limits remain **TODO / VERIFY on the physical board**.
