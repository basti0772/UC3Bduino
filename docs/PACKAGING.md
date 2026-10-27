# Platform and release packaging

Stage 7 keeps the working platform at `arduino/hardware/uc3bduino/avr32`.
`scripts/install.ps1` assembles it into the sketchbook's
`hardware/uc3bduino/avr32`, preserving the stable FQBN
**uc3bduino:avr32:uc3b0256**. The installed layout is:

```text
platform.txt, boards.txt, platform.local.txt, boards.local.txt, VERSION
cores/uc3bduino/          Arduino API + copied HAL/USB/startup
variants/uc3b0256/        explicit D0/UART selections
system/at32uc3b0256.ld    shared protected memory layout
tools/                   PowerShell DFU wrapper (no native binaries)
libraries/UC3BduinoExamples/examples/   all fourteen canonical sketches
docs/, LICENSE, CHANGELOG.md, shared-sources.json
```

This follows the current Arduino [platform specification](https://docs.arduino.cc/arduino-cli/platform-specification/):
vendor/architecture/board define the FQBN; `platform.local.txt` overrides compiler
and uploader paths; bundled libraries provide examples for the selected platform.
Existing compiler/archive/link/size and normal upload recipes are retained.
No unnecessary menus, external programmer or dummy `programmers.txt` is added:
normal factory DFU upload does not use Upload Using Programmer/Burn Bootloader.

Canonical shared files stay under `src`, `linker`, `scripts`, `docs` and `examples`.
The installer copies them and records source/installed names and SHA-256 in
`shared-sources.json`. Edit the canonical source and reinstall into a fresh
destination; do not maintain a second independent copy in the template.
Local path and hardware configurations are installation outputs, never release inputs.

## Tool integration

`build.ps1`, both installers and diagnosis share `installation-common.ps1`.
Selection is explicit `-ToolchainPath`, then `AVR32_TOOLCHAIN_PATH`, then PATH;
invalid explicit/environment settings fail. The installer records an absolute
`compiler.path` for Arduino, which does not depend on the IDE inheriting a shell's
environment. Diagnosis checks that recorded path. Direct GNU Make users can set
`TOOLCHAIN_BIN=<same-bin>`; the PowerShell build wrapper sets it automatically.
There is no CMake build to configure.

DFU selection uses explicit path, `UC3B_DFU_TOOL_PATH`, then PATH; the installer
validates 1.1.0 and records the executable. Compile-only installation is allowed
without DFU. The optional `get-dfu-tool.ps1` fetches and verifies the original
upstream asset without executing it. [THIRD_PARTY](THIRD_PARTY.md) records the
licenses and redistribution decision. Neither compiler nor DFU binaries are
included in UC3Bduino archives.

## Prepare an archive

```powershell
python tests/check_package.py
.\scripts\package-release.ps1
```

Output: `dist/0.8.0-dev/UC3Bduino-0.8.0-dev.zip`, `INSTALL_WINDOWS.md` and
`SHA256SUMS.txt`. The ZIP contains one `UC3Bduino-0.8.0-dev/` source root, all
installation documentation and an internal `SOURCE_SHA256SUMS.txt`. It works
without Git after extraction. `packaging/source-files.txt` is the reviewed source
allowlist; update it when adding files. The script reads current listed source
contents, including uncommitted edits, so use a reviewed clean commit for a
published artifact. Unlisted files and build/temp/tool outputs are never collected.
Do not put secrets in listed source files.

The packager refuses missing/duplicate/unsafe/reparse-point entries and existing
output directories. Repeating unchanged inputs under the same .NET runtime gives
identical archives (sorted entries, fixed timestamps); no cross-runtime compression
identity is promised. `-OutputDirectory` selects a new location. Packaging creates
no Git tag or GitHub release. The development archive is not a directly installable
Boards Manager platform archive; run `scripts/install.ps1` after extraction.

## Boards Manager and release gate

There are no published tags/releases or usable package index at Stage 7's start.
Private repository source access also does not automatically provide public,
unauthenticated asset downloads for Boards Manager. No fake URL/index is shipped.
Before generating an index, prepare an assembled portable platform archive,
verified downloadable tool archives for each supported host, licenses/notices and
corresponding source obligations; use the Arduino [package index specification](https://docs.arduino.cc/arduino-cli/package_index_json-specification/).
Each platform/tool needs real URL, archive name, exact size, SHA-256 and version;
`toolsDependencies` must match packager/name/version, with correct tool host triples.
Replace local absolute tool paths with actual installed tool dependency properties.
Validate the index and a clean Boards Manager install before advertising its URL.

`VERSION` and [CHANGELOG](../CHANGELOG.md) define Semantic Versioning. `0.8.0-dev`
continues the prior 0.5.0/0.6.0 platform progression; it is not a published release
or stable compatibility guarantee. Complete [HARDWARE_TEST](HARDWARE_TEST.md),
inspect all local regressions and licensing before publishing even the first
development release; reserve 1.0.0 for stable validated behavior.

## CI scope

`.github/workflows/build.yml` checks metadata/PowerShell syntax, assembles the source
archive and runs native host protocol/timing/UART/Print plus DFU-wrapper and installer
tests on Windows. Host fixtures never access real USB. The CI job name explicitly
says **no AVR32 compiler**. No compiler is downloaded, no firmware compile/upload
or hardware success is claimed; firmware regressions must run on a verified local
toolchain and be recorded in [TESTING](TESTING.md). No automatic release workflow
is enabled.
