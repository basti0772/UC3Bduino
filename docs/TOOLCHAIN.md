# AVR32 toolchain and build contract

## Tested baseline

The actual Stage 1 build used this locally installed Windows toolset:

| Program | Observed version / use |
| --- | --- |
| `avr32-gcc` | 4.3.2, `atmel-1.2.0-(mingw32_special)`; C and assembly driver, linker driver |
| `avr32-as`, `avr32-ld` | 2.19.20081016, same Atmel build |
| `avr32-objcopy`, `avr32-size` | Same binutils installation; HEX/BIN conversion and size report |
| GNU Make | 3.81, `i386-pc-mingw32` |
| `avr32-readelf`, `avr32-objdump`, `avr32-nm` | Same installation; optional inspection tools |
| `avr32-g++` | Companion C++ compiler; not invoked by this stage |

The package installer version was not established from the installed executables;
the compiler/binutils build strings above identify what was actually tested.
Keep a copy of your installer and its checksum outside this repository for
rebuilding on another machine. No automatic download from an unverified mirror.

The [Microchip toolchain archive](https://www.microchip.com/en-us/tools-resources/archives/avr-sam-mcus)
lists historical AVR32 distributions, including AVR32 GNU Toolchain 2.4.2 for
Windows and toolchain sources. A later official option is Atmel AVR 32-bit
Toolchain **3.4.3.820**; its
[release notes](https://ww1.microchip.com/downloads/aemDocuments/documents/OTH/ProductDocuments/UserGuides/avr32-gnu-toolchain-3.4.3.820-readme.pdf)
identify GCC 4.4.7, binutils 2.23.1, and Newlib 1.16.0.
**TODO / VERIFY:** run this repository's checks with 3.4.3 before treating it as
a tested alternative. Historical vendor OS support does not establish Windows 11
support; the validation record describes the concrete Windows 11 result here.

## Target selection

`tools/targets/at32uc3b0256.mk` owns `-mpart=uc3b0256` and the linker script path.
This is the spelling accepted by the installed compiler, without the `AT32`
prefix. Verification performed with the compiler itself:

```powershell
avr32-gcc --target-help
avr32-gcc -mpart=uc3b0256 -print-multi-directory
'' | avr32-gcc -mpart=uc3b0256 -dM -E -x c -
```

The multilib output is `ucr1`. Predefined macros include `__AVR32_UC3B0256__`,
`__AVR32_UC__`, `__AVR32_AVR32A__`, and `__AVR32__`.
This matches the device's architecture revision 1 (datasheet table 6-5).
No redundant `-march` or `-mcpu` is passed. In particular, ARM-style CPU names
and AVR 8-bit `-mmcu` options do not describe this target.

The installed GCC manual lists `-mpart=uc3b0256`; its architecture-name list is
older than `--target-help`. The executable's actual part selection and successful
compile/link are the decisive checks. The tested binutils writes ELF machine ID
`0x18ad`, an older AVR32 identifier. The verifier also accepts the assigned ID
185; both are recorded in the upstream
[Ghidra ELF constants](https://github.com/NationalSecurityAgency/ghidra/blob/master/Ghidra/Features/Base/src/main/java/ghidra/app/util/bin/format/elf/ElfConstants.java).
Use the matching `avr32-*` utilities to inspect these old ELFs.

## Flags actually used

| Flags | Purpose |
| --- | --- |
| `-mpart=uc3b0256` | Device and architecture selection in compiler, assembler and linker driver |
| `-mno-relax` | Disable link relaxation, keeping exception slot positions predictable |
| `-std=c99 -Os -g` | Conservative C dialect, size optimization, debug information |
| `-ffreestanding -fno-builtin` | No hosted C environment or builtin library assumptions |
| `-fno-common` | Diagnose duplicate globals; allocate uninitialized objects explicitly |
| `-ffunction-sections -fdata-sections` | Allow unused functions/data to be removed |
| `-muse-rodata-section` | Keep read-only data in `.rodata` |
| `-Wall -Wextra -Werror` | Strict diagnostics supported by GCC 4.3.2 |
| `-MMD -MP` | Local header dependency files |
| `-nostdlib -nostartfiles` | Exclude vendor CRT, libc and default libraries (including libgcc) |
| `-Tlinker/at32uc3b0256.ld` | Explicit project memory and section layout |
| `-Wl,--gc-sections` | Remove unreferenced input sections; startup/vectors/stack use `KEEP` |
| `-Wl,--no-undefined` | Reject unresolved runtime dependencies |
| `-Wl,-Map,...` | Write the linker map |
| `avr32-objcopy -O ihex` / `-O binary` | Generate HEX/BIN from flash load addresses |

All of these flags were exercised, not just inferred from another GCC target.
Assembly uses GCC preprocessing and the documented AVR32 `lda.w`, `call`, and
`.cpool` facilities. The installed `share/info/as.info`, section 9.5, explains
these pseudoinstructions. `share/man/man1/avr32-gcc.1` documents AVR32 options.

`-nostdlib` means future division, floating-point, aggregate copies, or other
operations can expose missing helper functions. Do not silently add libc or
libgcc: review the required runtime, licensing and startup contract first.

## Headers and external components

No device header is required by this stage. Only documented CPU fields are named
locally, and only the linker defines the chip's memory geometry. Future peripheral
code can investigate the official toolchain's `avr32/include/avr32/io.h`, selected
`uc3b0256.h`, and associated module headers or the archive's header packages.
Each exact package needs its own license review; see [THIRD_PARTY](THIRD_PARTY.md).

Build dependencies are the standalone AVR32 toolchain, GNU Make, and PowerShell.
No ASF, Studio project, IDE, Newlib headers/libraries, SDK download, or Python is
required for the firmware build. The ELF verifier uses only Python's standard library.

## Why Make in Stage 1

The installed historical toolchain already includes GNU Make 3.81, while CMake
is not installed in the validation environment. Two objects and a custom linker
script need no compiler identification or hosted-C compiler probes. A small
Makefile uses the existing tools directly, and the PowerShell wrapper handles
Windows discovery, quoting, output-directory creation, and errors.
CMake remains a possible later replacement; it is not known to be incompatible.

Direct GNU Make use (create the output directory first, tools on PATH):

```powershell
New-Item -ItemType Directory -Force build/at32uc3b0256 | Out-Null
make -B all
```

The Windows wrapper is the supported entry point and uses separate example
directories. Always use `-B` for direct Make builds when switching toolchains,
flags or application sources. `clean.ps1` only removes the repository's `build`
tree, refuses reparse points, and supports `-WhatIf`.
