# AVR32 toolchain and build contract

## Tested baseline

Stages 1, 2 and 2.5 used this locally installed Windows toolset:

| Program | Observed version / use |
| --- | --- |
| `avr32-gcc` | 4.3.2, `atmel-1.2.0-(mingw32_special)`; C and assembly driver, linker driver |
| `avr32-as`, `avr32-ld` | 2.19.20081016, same Atmel build |
| `avr32-objcopy`, `avr32-size` | Same binutils installation; HEX/BIN conversion and size report |
| GNU Make | 3.81, `i386-pc-mingw32` |
| `avr32-readelf`, `avr32-objdump`, `avr32-nm` | Same installation; optional inspection tools |
| `avr32-g++` | 4.3.2, same Atmel build; C++98 sketches/core and Arduino link driver in Stage 2.5 |
| Arduino CLI | 1.5.2-rc.1, commit `fef6e48df`; Stage 2.5 discovery and sketch builds |

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

## Stage 7 packaging assessment (2026-10-26)

The archive's [2.4.2 Windows installer](https://ww1.microchip.com/downloads/archive/avr32-gnu-toolchain-2.4.2-setup.exe)
responded HTTP 200 to HEAD with **51,312,781 bytes**. It was not installed during
this stage and no installer SHA-256 is asserted. The existing tested installation
contains **1,907 files / 187,477,307 bytes** (sum of file lengths, not allocated
disk space). The [2.4.2 release notes](https://ww1.microchip.com/downloads/en/DeviceDoc/AVR32%20GNU%20Toolchain%202.4.2.pdf)
list GCC 4.3.2-atmel.1.2.0, binutils 2.19.atmel.1.2.0, Newlib 1.16.0 and headers
2.3.4, consistent with the compiler family here but not proof of the installed
package's provenance. They require at least 250 MB free space and list Windows
2000/XP/Vista and historical Linux distributions. Their 64-bit support statement
is negative; local Windows 11 build success is an empirical result, not vendor support.

Microchip lists 2.4.2 Linux packages for old Fedora/Ubuntu/SUSE/Red Hat systems and
3.4.3 Linux 32/64-bit packages. Those are **not tested alternatives** on a current
Linux distribution. Installer and upload integration in this release target Windows;
do not infer Linux compatibility from the existence of a vendor download. The archive
page's 2.4.2 "Linux Source Code" link currently points to the same Windows EXE;
it is not evidence of a recovered corresponding-source archive.

Required executables are gcc, g++, as, ld, ar, objcopy and size (all with the
`avr32-` prefix). Make is needed only for the bare-metal wrapper. Keep the full
installation, including device/standard headers and compiler internals; copying
only these EXEs is insufficient. Path resolution is shared by the Windows scripts:
explicit `-ToolchainPath` > `AVR32_TOOLCHAIN_PATH` > PATH. Root/bin are accepted;
all tools plus target/multilib and C/C++ version consistency are checked.
Arduino receives the chosen absolute bin in `platform.local.txt`; diagnosis
checks that installed choice. Direct Make accepts `TOOLCHAIN_BIN` for the same bin.

GCC/binutils and other included components have separate redistribution terms,
and the original installer/corresponding sources have not been fully audited.
The compiler stays external; no compiler auto-download, binary redistribution
or blanket non-redistributability claim is made. See [THIRD_PARTY](THIRD_PARTY.md).

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
| `-fms-extensions` (Make HAL objects; Arduino C recipe) | Enable anonymous struct/union fields in official Atmel headers under GCC 4.3 C99 |
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

The HAL includes installed `avr32/uc3b0256.h` directly, after checking the
compiler's target macro. This transitively includes PM/GPIO and other module
definitions. Anonymous unions in these headers require `-fms-extensions` with
the tested C99 compiler; without it GCC reports missing register members.
Make confines the option to HAL objects. The Arduino C recipe uses it for all C
sources because recipes select by language, not source directory. C++ uses its
own flags. This enables a language extension; it does not suppress warnings.
`uc3b_device.h` checks used register offsets and bank stride at compile time.
See [THIRD_PARTY](THIRD_PARTY.md) for versions, hashes and notices.

Bare-metal build dependencies are the standalone AVR32 toolchain, GNU Make, and PowerShell.
Installed standard headers `stdint.h` (Newlib) and `stdbool.h` (GCC) provide
types; no runtime library is linked. No ASF, Studio project, IDE, separate SDK
download or Python is required for firmware builds. The ELF verifier uses only
Python's standard library.

## Arduino C++98 build

The local platform reuses the target, optimization, section, diagnostic and
freestanding flags above. Its C++ recipe adds `-std=c++98`, `-fno-exceptions`,
`-fno-rtti`, `-fno-threadsafe-statics` and `-fno-use-cxa-atexit`. No modern C++
features or STL are assumed. Nontrivial global/static object initialization is
unsupported; the linker rejects constructor sections, and no allocation or
destructor runtime is supplied.

Arduino CLI handles .ino preprocessing and prototype generation. `avr32-ar`
builds `core.a`; the G++ link driver uses `--undefined=_start` and an archive
group to retain the shared reset/main chain. `-nostdlib -nostartfiles` still
excludes automatic libraries and CRT. The original linker script remains the
authority for flash/SRAM layout and constructor rejection.

The installer resolves `AVR32_TOOLCHAIN_PATH`, an explicit `-ToolchainPath`, or
PATH into a generated `platform.local.txt`. No compiler distribution is bundled.
Arduino CLI's own indexes and builtin preprocessing tools are external setup
requirements; GNU Make is not used by this path. See
[installation](ARDUINO_IDE.md) and [executed checks](TESTING.md).

## Why Make

The installed historical toolchain already includes GNU Make 3.81, while CMake
is not installed in the validation environment. Four objects and a custom linker
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
