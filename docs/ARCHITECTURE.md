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
board variants will describe wiring. These are planned boundaries, not existing
modules or promises that all UC3B devices have identical registers.

Stage 1 contains only a reset runtime, a device linker script, build configuration
and a minimal `main`. Startup uses linker symbols rather than embedded flash/RAM
sizes. The target make fragment chooses the compiler part and linker script.
AT32UC3B0256-specific values are confined to that fragment, the linker script and
the explicit validation expectations. Adding another chip requires a verified
memory map, compiler target, startup/errata review and separate validation.

The current startup has no constructors, destructor dispatch, dynamic allocation,
interrupt abstraction, peripheral initialization or HAL. These contracts should
be introduced only when their stage requires them. In particular, C++ support
must be designed against the selected historical compiler before adopting modern
language features or an Arduino class implementation.
