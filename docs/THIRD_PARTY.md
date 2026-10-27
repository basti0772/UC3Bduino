# Third-party components and licensing

## What is in this repository

New source, assembly, linker script, build scripts, tests and documentation are
original project work under the MIT license in `LICENSE`. No vendor startup,
linker script, generated header, ASF source, binary, or standard-library code is
copied into the repository. Hardware values were checked against the primary
references in [HARDWARE](HARDWARE.md); vendor documents are linked, not bundled.

## External tools and reviewed headers

Stage 6 is an original implementation based on the hardware and USB protocol
documents linked in [USB_CDC](USB_CDC.md). No ASF code was imported. Before
using ASF as a technical reference, the following files were inspected at
revision `68cddb46ae5ebc24ef8287a8d4c61a6efa5e2848` of the
[ASF mirror](https://github.com/avrxml/asf/tree/68cddb46ae5ebc24ef8287a8d4c61a6efa5e2848):

| File | Original license / use |
| --- | --- |
| `avr32/drivers/usbb/usbb_device.c` | Microchip 2009–2018 restricted product-use terms; reference only, no code copied |
| `avr32/drivers/usbb/usbb_device.h` | Same Microchip restricted product-use terms; reference only, no code copied |
| `common/services/usb/class/cdc/device/udi_cdc_desc.c` | Same Microchip restricted product-use terms; descriptor/reference inspection only, no code copied |

Those notices restrict use of the software and derivatives to Microchip
products; they are not treated as an unrestricted MIT-compatible grant.
Register facts, FIFO ordering and USB/CDC encodings were independently checked
against 32059L and USB-IF specifications. The separately installed `usbb_310.h`
already belongs to the Stage 2 transitive device-header manifest and BSD-style
notice review below. Vendor specifications and downloaded reference sources
are not redistributed in this repository.

Stage 5 uses separately installed **dfu-programmer 1.1.0** (GPL-2.0-or-later),
with its shipped libusb and runtime licenses. No executable, DLL or source is
redistributed here. `src/arguments.c`, `commands.c`, `atmel.c`, `dfu.c` and
`intel_hex.c` were inspected as protocol/CLI references; no implementation code
was copied. Release/source links are in [DFU_UPLOAD](DFU_UPLOAD.md).
Legacy Atmel `conf_isp.h` / `usb_dfu.c` (Atmel BSD-style three-clause notices)
and ASF `common/services/isp/flip/uc3/isp.c` (Microchip restricted product-use
license in the inspected revision) were consulted for signature formats only.
No source from those files was imported. The PowerShell uploader is original
project code; chip IDs and protocol facts are documented with source links.

The legacy Atmel AVR32 GNU distribution includes GCC, binutils, GNU Make,
Newlib and vendor device support. These components retain their own licenses.
They are installed separately and are not relicensed by this project's MIT file.
The build uses `-nostdlib -nostartfiles`: no Newlib, libgcc or vendor CRT is linked.
The resulting program has no unresolved library symbols.

Stage 2 compiles against installed `avr32/include/avr32/uc3b0256.h` and its
transitive module headers, including `pm_230.h`, `gpio_110.h` and `abi.h`. All
22 inspected Atmel headers declare revision 62595 and check-in 2009-10-21.
Their license conditions and disclaimer were compared and match; copyright
years vary by file. The notice permits redistribution with source/binary notice
retention and a non-endorsement clause (three-clause BSD-style terms).

The [manifest](DEVICE_HEADERS.md) records every included vendor header and its
SHA-256, plus standard `stdint.h`, `stdbool.h` and `stddef.h`. The original installer
archive/version is unknown; executable versions and file hashes identify the
tested installation without claiming a newer device pack was used. Compiler
dependency output (`avr32-gcc -M`) was checked against this list.

`stdint.h` has a permissive notice from Ralf Corsepius requiring notice retention.
GCC's `stdbool.h` and Stage 4's `stddef.h` carry GPL terms with an explicit exception for inclusion in
GCC-compiled source. The [retained notices](THIRD_PARTY_NOTICES.md) include these
texts and the Atmel conditions. Keep them with distributed firmware/documentation.
Headers provide types/register definitions; no Newlib or GCC runtime is linked.

## Stage 7 distribution inventory (2026-10-26)

| Component / version | Source and license | UC3Bduino distribution decision |
| --- | --- | --- |
| AVR32 GCC/G++ 4.3.2 atmel-1.2.0 | [Microchip archive](https://www.microchip.com/en-us/tools-resources/archives/avr-sam-mcus); GCC 4.3 is [GPLv3 or later](https://github.com/gcc-mirror/gcc/blob/releases/gcc-4.3.2/COPYING3), with separate header/runtime exceptions where applicable | External executable only; original package and corresponding-source provenance still incomplete. Do not bundle the whole toolchain under an assumed single license. |
| AVR32 binutils 2.19.20081016 | Same installed Atmel distribution; `avr32-ld --version` explicitly declares GPLv3 or later | External, not bundled. GPL redistribution is conditional on the applicable notices/source obligations. |
| GNU Make 3.81 | Installed toolchain `contrib/make/3.81/make-3.81-src/COPYING`, GPLv2 or later | External bare-metal build tool, not bundled. |
| Newlib 1.16.0 family and vendor device headers | [Toolchain release notes](https://ww1.microchip.com/downloads/en/DeviceDoc/AVR32%20GNU%20Toolchain%202.4.2.pdf); mixed per-file permissive terms; the exact included header notices/hashes are retained in DEVICE_HEADERS/THIRD_PARTY_NOTICES | Runtime not linked; no header source copied into the archive. Retain notices with firmware as already required. No blanket approval for all Newlib/vendor package contents. |
| dfu-programmer 1.1.0 x64 | [Official release](https://github.com/dfu-programmer/dfu-programmer/releases/tag/v1.1.0), [COPYING](https://github.com/dfu-programmer/dfu-programmer/blob/v1.1.0/COPYING), GPL-2.0-or-later | External upstream download, size 423421 bytes and measured SHA-256 in INSTALL_WINDOWS. Optional helper verifies/extracts the original asset, never repackages it. |
| libusb/runtime DLLs shipped with dfu-programmer | Upstream ZIP's `licenses/` contains libusb LGPL-2.1-or-later, GCC runtime GPLv3 + runtime exception, MinGW CRT/winpthreads and other component notices | Preserve the complete upstream ZIP/extraction. Do not strip licenses or assume the EXE's GPL covers all DLLs. No DLLs enter UC3Bduino source releases. |
| Atmel register headers, revision 62595; ASF USB reference revision above | Atmel three-clause BSD-style terms for the inspected headers; separate Microchip restricted terms for inspected ASF USB source | Reference/include use described above; no ASF code import, no new third-party source in Stage 7. |

GCC's [runtime library licensing guidance](https://www.gnu.org/licenses/gcc-exception-3.1-faq.html)
does not replace the exact license/exception of this old installation. This core
links no GCC runtime/Newlib/CRT. The project's MIT LICENSE remains appropriate
for its original files; external compiler output/header notices remain separate.
No incompatible copied source was identified in the reviewed repository.
Tool redistribution is deferred for provenance/source-compliance review, **not**
because all these open-source tools are intrinsically non-redistributable.

## TODO / VERIFY when changing dependencies

- Recover the original installer version/checksum when available. When changing
  toolchains, review every included file's notice and dependencies and regenerate
  the manifest; these findings do not automatically cover another package.
- Preserve any required source and binary notices in a dedicated third-party
  directory and release documentation.
- Review runtime-library terms/exceptions before adding libgcc, Newlib, C++
  startup, or software floating-point code to the link.
- Do not copy ASF startup or GPIO code merely because a local example builds.

Project MIT licensing does not replace the external header notices.
