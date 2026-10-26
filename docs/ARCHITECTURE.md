# Architecture direction

**Stage 6 – USB CDC / SerialUSB**

The single linker now reserves the factory bootloader's first 8 KiB of flash
and the first SRAM word. `_start` is the application entry at `0x80002000`.
The canonical PowerShell uploader is copied into the local Arduino package;
manual, CLI and IDE uploads all use this implementation. It checks HEX addresses,
USB identity and BOOTPROT before erase, then programs with readback and requests
a reset-based application start. See [DFU_UPLOAD](DFU_UPLOAD.md).

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
headers provide register layouts. General resource allocation and a complete
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
the documented Rev B erratum. TC0, UART and USB have entries at 0x104/0x108/0x10c;
linker assertions enforce EVBA-OR-offset addressing after relocating the image.
Clock setup is a reset-time operation with interrupts masked. GPIO alias writes
are atomic per register, but callers serialize configuration/ownership changes.
The pin-validity helper is hardware-independent and can be host-tested later;
GPIO remains independent of the timing component.

## Arduino integration

`arduino/hardware/uc3bduino/avr32` is the platform template. Its `boards.txt`
selects the one MCU, core and variant. `platform.txt` mirrors the bare-metal
target/section/freestanding flags and adds C++98 compilation and an Arduino core
archive. `main.cpp` initializes the clock and system timer, calls `setup()` once,
then calls `loop()` forever. HAL functions retain C linkage through guarded
declarations; setup/loop retain C++ linkage.

The installer creates a self-contained local Arduino package by copying the
canonical `src/hal`, `src/usb`, `src/startup/uc3_startup.S` and
`linker/at32uc3b0256.ld` files into Arduino's expected build directories. It records
SHA-256 hashes in `shared-sources.json`. No duplicate startup or linker source
is maintained in Git. Shared changes require reinstalling the snapshot. A
generated `platform.local.txt` selects the installed toolchain's absolute bin
path, including paths with spaces. `boards.local.txt` records explicit OSC0
clock/F_CPU and ring-size flags; `serial_config.h` records the selected route.

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
allocation or general interrupt abstraction. The shared linker rejects constructor
sections rather than silently omitting their execution. Sketches use the tested
historical compiler's C++98 subset, without exceptions, RTTI or STL.

`wiring_time.cpp` delegates time reads to the C system-timer HAL and waits using
unsigned elapsed subtraction. TC0/INTC group 14 are reserved. The private
hardware adapter owns register access and interrupt masking; the C logic owns
the two epochs and coherent CV/status snapshots. Short critical sections preserve
the caller's mask, and conversion runs after restoring it. Native tests execute
the real C logic with scripted register events and the real C++ wait wrappers
with a wrapping clock. They do not simulate AVR32 hardware. See [TIMING](TIMING.md)
for service-latency, precision and clock-change constraints.

## HardwareSerial and interrupt ownership

`HardwareSerial.cpp` owns the Arduino adapter and sole global `Serial` object.
Its constructor-free `Print` base formats decimal integers using ten stack bytes
and a byte-writer callback; no vtable, sprintf or runtime library is needed.
`begin()` binds that callback only after successful HAL configuration. This is
a deliberately small API subset, not Arduino's complete virtual Print/Stream ABI.

`uc3b_uart.c` owns one static RX ring and foreground API. Its private hardware
adapter maps an explicitly selected MCU route to USART registers/clock/INTC
group and delegates mux operations to `uc3b_gpio_select_peripheral`. No board
connector mapping is embedded. The unconfigured route leaves UART inactive.
UART uses INT1 and timer INT0; selected USART group 5/6/7 and TC group 14 are
reserved. Both use `uc3b_irq.h` for GM save/restore. RX head/tail operations are
briefly masked; TX polling never masks while waiting. Hardware errors and full
buffers discard new bytes. No DMA or general dispatcher is added.

115200 baud cannot come from default RCSYS. The shared clock HAL therefore
offers opt-in OSC0, explicitly configured before timer initialization, without
a PLL or live frequency changes. CPU/HSB/PBA/PBB remain equal. U0A conflicts
with OSC0 and is rejected. Existing default RC behavior and bare-metal builds
are preserved. See [SERIAL](SERIAL.md) for registers, ownership, wiring, rates,
callback limitations and pending hardware validation.

## USB CDC

`USBSerial` shares the existing Print sink ABI. `usb_cdc.c` owns line coding,
control lines and static RX/TX rings; `usb_device_core.c` owns descriptors and
the EP0 state machine; `uc3b_usb_device.c` owns USBB registers, FIFO allocation,
interrupt acknowledgement and attach/detach. Central settings are in
`usb_config.h`. Native tests replace the register transport, exercising the
actual portable core/class and Arduino adapter separately.

USBB uses INTC group 17/INT2, with bounded packet work and no Arduino formatting.
The shared clock HAL supplies PLL1/GCLK3 at 48 MHz from explicit 8/12/16 MHz
OSC0, preserving CPU/bus clocks. No general interrupt dispatcher, ASF, DMA,
heap, constructor, UART bridge or bootloader transition command is added.
VID/PID defaults to unconfigured, so no application USB attachment occurs until
the developer supplies authorized IDs. Details and hardware limits are in
[USB_CDC](USB_CDC.md).
