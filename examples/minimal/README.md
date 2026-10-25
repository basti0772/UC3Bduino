# Minimal firmware

The single source of truth is [`src/main.c`](../../src/main.c).
It initializes the undivided internal RC clock and then loops forever. It does
not select or drive any pin and does not require a board definition.

From the repository root on Windows:

```powershell
.\scripts\build.ps1
```

Output: `build/at32uc3b0256/minimal/firmware.{elf,hex,bin,map}`.
See [validation](../../docs/VALIDATION.md) for what this build does and does
not prove. No flashing command is provided.
