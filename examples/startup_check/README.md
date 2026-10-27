# Startup initialization fixture

```powershell
.\scripts\build.ps1 -Example startup-check
python tests/verify_firmware.py build/at32uc3b0256/startup-check --startup-check
```

`main.c` deliberately keeps seven initialized bytes in `.data`, seven bytes in
`.bss`, and a result variable in `.bss`. The unaligned array sizes also exercise
the startup loops' trailing word padding. The ELF verifier checks that the
initializer exists in the flash load image; it does not execute the CPU.

For a later hardware test, use a supported debugger and an explicitly approved
flashing procedure. Break at `_start`, deliberately fill the `.data` and `.bss`
SRAM ranges with nonzero bytes, then continue from reset code to `main`. Do not
let a debugger pre-initialize RAM and skip the startup under test. Check the
copy and zero results; after continuing, inspect `startup_check_status`:

- `0`: check has not completed.
- `1`: the fixture observed the expected initial values.
- `2`: an initialization check failed.

**TODO / VERIFY:** execute this test on AT32UC3B0256 hardware. No physical device
or simulator execution was performed in Stage 1.
