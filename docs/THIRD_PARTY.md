# Third-party components and licensing

## What is in this repository

New source, assembly, linker script, build scripts, tests and documentation are
original project work under the MIT license in `LICENSE`. No vendor startup,
linker script, generated header, ASF source, binary, or standard-library code is
copied into the repository. Hardware values were checked against the primary
references in [HARDWARE](HARDWARE.md); vendor documents are linked, not bundled.

## External tools and reviewed headers

The legacy Atmel AVR32 GNU distribution includes GCC, binutils, GNU Make,
Newlib and vendor device support. These components retain their own licenses.
They are installed separately and are not relicensed by this project's MIT file.
The build uses `-nostdlib -nostartfiles`: no Newlib, libgcc or vendor CRT is linked.
The resulting program has no unresolved library symbols.

For provenance checking, the locally installed
`avr32/include/avr32/uc3b0256.h` (Atmel copyright 2006–2009, revision 62595,
2009-10-21) was inspected. Its notice permits redistribution with attribution,
binary-distribution notice retention and a non-endorsement condition (a
three-clause BSD-style notice). It is **not included or compiled** by Stage 1.
This observation applies only to that particular header, not automatically to
other headers, newer device packs or ASF releases.

## TODO / VERIFY before adding vendor dependencies

- Record the exact source package, version and checksum; review every included
  file's notice and dependencies, not only the top-level archive license.
- Preserve any required source and binary notices in a dedicated third-party
  directory and release documentation.
- Review runtime-library terms/exceptions before adding libgcc, Newlib, C++
  startup, or software floating-point code to the link.
- Do not copy ASF startup or GPIO code merely because a local example builds.

No unresolved third-party source license is required for the current build.
