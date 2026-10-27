# Troubleshooting

Start with `scripts/diagnose.ps1 -SketchbookPath <actual-IDE-sketchbook>`.
It does not flash or query the DFU protocol. Compiler logs are retained in the
printed build directory. Use `-CliPath` / `-ConfigFile` when Arduino CLI and IDE
use different installations. See [INSTALL_WINDOWS](INSTALL_WINDOWS.md).

| Symptom | Concrete checks |
| --- | --- |
| Board not visible in IDE | Check IDE Preferences > Sketchbook location. It must contain `hardware/uc3bduino/avr32/boards.txt` and `platform.txt`. Restart the IDE. Use `arduino-cli board listall UC3Bduino` with the same `ARDUINO_DIRECTORIES_USER`. Installing a source ZIP through Add .ZIP Library is wrong. |
| `avr32-g++` not found | Set `AVR32_TOOLCHAIN_PATH` to the complete legacy AVR32 root/bin, or pass `-ToolchainPath`. Check the installed `platform.local.txt` after moving tools. AVR 8-bit/Arm tools do not substitute. An explicit invalid path never falls back silently to PATH. |
| Compiler rejects headers/target | Keep vendor headers and subdirectories with the binaries. `avr32-gcc -dumpmachine` must be AVR32; `-mpart=uc3b0256 -print-multi-directory` must return `ucr1`. Compare versions and header manifest in TOOLCHAIN/DEVICE_HEADERS. Newer compiler distributions require revalidation. |
| Installer says destination exists | Nothing is overwritten. Back up/move only the old UC3Bduino platform outside the hardware tree, or use a fresh sketchbook. Keep your old local hardware options for reinstall. |
| Blink compile requests D0 | Reinstall with both `-GpioPort` and `-GpioPin` from the schematic. `ArduinoMinimal` is the no-pin compiler check. PA00..02/JTAG, OSC0 and selected UART conflicts are rejected. |
| DFU device not detected | Use a data cable; inspect Device Manager for hardware ID **03EB:2FF6**. Check supplies, supported OSC0 crystal and board-specific reset/boot pin sequence. CDC COM is a different device. PnP presence does not prove libusb access. |
| Upload fails | Retain the full log. Use dfu-programmer **1.1.0**, its DLLs, the correct DFU WinUSB driver and only one connected DFU board. Check exact MCU marking before `-LegacyDevice AT32UC3B0256`. Do not bypass identity, BOOTPROT=3 or address-range checks. No fuse repair or chip erase. |
| Blink does not run | Confirm normal Upload completed through launch; after failed DFU queries ISP_FORCE can keep the board in DFU. Check boot pin released, clock, actual pad mapping, LED polarity/resistor and measure D0. Confirm firmware came from the protected `0x80002000` application build. |
| CDC COM port absent | Confirm application launched, configured authorized VID/PID and actual 8/12/16 MHz OSC0. With zero IDs SerialUSB intentionally never attaches. Check D+/D-, VBUS, cable and Device Manager; do not install the DFU WinUSB driver on CDC. |
| USB device descriptor failed | Return to physical USB/clock/supply checks. Record hardware IDs and Windows status; test another data cable/port, avoid hubs for initial checks. Confirm only one application is using your assigned VID/PID. Run USBSerialHello with the same verified configuration. Enumeration is still a pending hardware test. |
| SerialUSB produces no output | Select the application COM port and open the monitor so DTR is set. `if (SerialUSB)` requires configuration, awake USB and DTR. UART `Serial` is separate; monitor baud does not configure a UART. Use repeated USBSerialBlink output, then Echo, not only a one-shot startup line. |
| CLI compiles but IDE fails | Compare sketchbook, compiler override, CLI config and actual IDE executable. Restart after environment/config changes. Use the IDE's verbose compilation/upload logs. Local installation is a snapshot; source edits require reinstall. |

The detailed [DFU guide](DFU_UPLOAD.md), [USB CDC contract](USB_CDC.md),
[clock guide](CLOCK.md) and [UART guide](SERIAL.md) explain the constraints.
Linux packaging/upload and Arduino IDE GUI behavior have not been verified.

## Useful bug report

Include UC3Bduino version (`VERSION` / installed `platform.txt`), Arduino IDE and
CLI versions, Windows version, `avr32-gcc --version`, dfu-programmer version,
controller marking/revision and board schematic or exact board name. Add the
selected GPIO/UART/OSC0 settings, application VID/PID, DFU hardware ID/driver,
diagnostic output, example name, full compile/upload logs and expected/observed
behavior. Remove personal paths or serial numbers before sharing publicly.
State whether the failure occurred during compilation, DFU upload, launch or
CDC enumeration; a successful compilation alone is not a hardware test.
