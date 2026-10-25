# Architecture direction

**Stage 2.5 – Arduino IDE compilation smoke test**

```text
Arduino API
    |
    v
UC3Bduino Core
    |
    v
UC3B HAL
    |
    v
AT32UC3B hardware registers
```

The minimal Arduino API exposes setup/loop and digital GPIO functions. The core
owns their semantics and maps logical pins through the variant table. The UC3B
HAL implements MCU operations independently of Arduino names; installed device
headers provide register layouts. Classes, resource allocation and a complete
board wiring description remain future work. Other UC3B devices need separate
verification.

Stage 1 established a reset runtime, a device linker script, build configuration
and a minimal `main`. Startup uses linker symbols rather than embedded flash/RAM
sizes. The target make fragment chooses the compiler part and linker script.
AT32UC3B0256-specific values live in that fragment, the linker script, HAL and
explicit validation expectations. Adding another chip requires a verified
memory map, compiler target, startup/errata review and separate validation.

Stage 2 adds `src/hal/uc3b_clock.[ch]` and `uc3b_gpio.[ch]`. The internal
`uc3b_device.h` gates the compiler target and checks the installed official
header layout. GPIO's public type contains only a register-bank index and bit
number; no board mapping is embedded. `minimal` and `gpio-toggle` initialize
the clock explicitly in `main`; `startup-check` retains its memory-only purpose.
All HAL functions compile, while section garbage collection removes unused ones.

The startup and linker contracts remain intact. Two NOPs after masking GM apply
the documented Rev B erratum; no interrupt initialization or dispatch is added.
Clock setup is a reset-time operation with interrupts masked. GPIO alias writes
are atomic per register, but callers serialize configuration/ownership changes.
The pin-validity helper is hardware-independent and can be host-tested later;
this stage adds no mocking framework.

## Arduino integration

`arduino/hardware/uc3bduino/avr32` is the platform template. Its `boards.txt`
selects the one MCU, core and variant. `platform.txt` mirrors the bare-metal
target/section/freestanding flags and adds C++98 compilation and an Arduino core
archive. `main.cpp` initializes the clock, calls the sketch's `setup()` once,
then calls `loop()` forever. HAL functions retain C linkage through guarded
declarations; setup/loop retain C++ linkage. The only change to shared HAL code
in this stage is these header guards.

The installer creates a self-contained local Arduino package by copying the
canonical `src/hal`, `src/startup/uc3_startup.S` and
`linker/at32uc3b0256.ld` files into Arduino's expected build directories. It records
SHA-256 hashes in `shared-sources.json`. No duplicate startup or linker source
is maintained in Git. Shared changes require reinstalling the snapshot. A
generated `platform.local.txt` selects the installed toolchain's absolute bin
path, including paths with spaces.

The linker uses `--undefined=_start` to extract reset code from `core.a`, which
then references main. The same memory and unsupported-runtime assertions apply
to both builds. No vendor CRT, libc, libgcc or C++ runtime is linked. See
[TOOLCHAIN](TOOLCHAIN.md) for the C++ flags and [TESTING](TESTING.md) for actual
CLI archive/link and artifact checks.

`wiring_digital.cpp` checks the logical index before accessing `uc3b_pin_map`,
validates the mapped HAL pin, and delegates every GPIO operation to the C HAL.
The variant offers one logical slot, D0, initially invalid. Installation can map
it explicitly to a board-verified pad. No LED or full pinout is invented. Invalid
pins have no GPIO effect and read LOW; unsupported modes have no effect. INPUT,
OUTPUT (initial LOW) and INPUT_PULLUP are supported. Writing HIGH to an input
does not enable its pull-up; use INPUT_PULLUP. These limited semantics are
documented in [ARDUINO_IDE](ARDUINO_IDE.md).

The current runtime has no global constructors, destructor dispatch, dynamic
allocation or interrupt abstraction. The shared linker rejects constructor
sections rather than silently omitting their execution. Sketches use the tested
historical compiler's C++98 subset, without exceptions, RTTI or STL. Timing,
Serial and other Arduino facilities remain separate stages; a real hardware
timer and its interrupt/ownership design are the next step.
