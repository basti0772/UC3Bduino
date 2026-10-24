# Stage 1 validation

## Executed on 2026-10-24

Host: Windows 11, Windows PowerShell 5.1 and PowerShell 7. The installed Atmel
toolchain reports GCC 4.3.2 `atmel-1.2.0-(mingw32_special)`, binutils
2.19.20081016 and GNU Make 3.81. Python 3.14 ran the optional verifier.

The 32-bit legacy executables initially failed to start inside the Codex sandbox
with a Windows DLL relocation error. They ran successfully in the authorized
host execution context. Windows PowerShell 5.1 also required the process-local
`-ExecutionPolicy Bypass` invocation shown in the README; no persistent host
policy was modified. These are environment observations, not compiler failures.

Both examples were actually compiled, assembled and linked with the documented
flags, and converted to ELF, HEX, BIN and map output:

| Example | `avr32-size` text | data | bss (includes stack) | BIN bytes |
| --- | ---: | ---: | ---: | ---: |
| minimal | 600 | 0 | 1024 | 1028 |
| startup-check | 736 | 8 | 1036 | 1172 |

The 1024-byte stack is allocated `NOBITS`, not flash payload. Flash alignment gaps
account for the difference between text+data and BIN length.

Checks performed:

- `avr32-readelf`: big-endian ELF32, AVR32 machine, entry `0x80000000`.
- `avr32-objdump`: SP loads `0x8000`; EVBA loads `0x80000200`; word copy/clear
  loops precede the call to `main`; exception offsets contain self branches.
- `avr32-nm -u`: no undefined symbols. Link map contains the two project objects,
  without CRT, libc or libgcc archive members.
- Independent `tests/verify_firmware.py`: flash load segments, SRAM bounds,
  allocated NOBITS stack, HEX checksums and byte-for-byte ELF/HEX/BIN agreement.
- Nonempty `.data` (8 bytes including padding) and `.bss` (12 bytes) in the
  startup fixture; its seven initializer bytes are present at the flash LMA.
- Windows PowerShell 5.1: explicit toolchain root, environment-specified bin
  directory, invocation from another directory, checkout path with spaces.
- Repeated minimal builds produced identical ELF, HEX, BIN and map hashes in
  the same checkout with the same compiler. Cross-machine/path-independent
  debug information is not claimed.
- Invalid explicit toolchain and missing PATH toolchain fail with nonzero exit
  status and actionable messages.
- Deliberate SRAM/stack overlap, oversized flash payload and unresolved runtime
  symbol fixtures fail to link. No false success is reported by the wrapper.
- `clean.ps1 -WhatIf` preserves output; normal clean removes only the temporary
  checkout's `build` tree and leaves its source files in place.

The extra error fixtures and logs are local, ignored scratch files. The permanent
artifact verifier and startup-check example provide repeatable positive checks.

## Reproduce

```powershell
.\scripts\build.ps1
.\scripts\build.ps1 -Example startup-check
python tests/verify_firmware.py build/at32uc3b0256/minimal
python tests/verify_firmware.py build/at32uc3b0256/startup-check --startup-check
avr32-readelf -h -S -l build/at32uc3b0256/startup-check/firmware.elf
avr32-objdump -d -j .reset -j .exception_vectors build/at32uc3b0256/startup-check/firmware.elf
avr32-nm -u build/at32uc3b0256/startup-check/firmware.elf
```

The inspection commands require the toolchain's bin directory on the caller's
PATH; `build.ps1` only changes PATH within its own invocation.

## Not executed / TODO / VERIFY

- No firmware was flashed; no physical board or AVR32 simulator was run.
- Stack behavior, memory initialization and exception entry on actual silicon
  still need the [debugger fixture procedure](../examples/startup_check/README.md).
- Board-specific watchdog/fuses, oscillator wiring, bootloader coexistence,
  programming hardware and silicon errata remain to be established.
- Atmel toolchain 3.4.3.820 and Linux builds have not been tested.
- No Arduino API, peripheral behavior, timing accuracy or C++ runtime is claimed.
