# Architecture direction

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

The Arduino API will expose familiar functions and classes. The core will own
Arduino semantics, resource allocation and the mapping from logical pins to
board capabilities. The UC3B HAL will implement MCU operations independently of
Arduino names. Device definitions will provide register layouts and capabilities;
board variants will describe wiring. The small HAL now exists; the Arduino/core
and board layers remain planned. Other UC3B devices need separate verification.

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

The current runtime has no constructors, destructor dispatch, dynamic allocation
or interrupt abstraction. These contracts should be introduced only when their
stage requires them. In particular, C++ support
must be designed against the selected historical compiler before adopting modern
language features or an Arduino class implementation.
