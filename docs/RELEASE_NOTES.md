# UC3Bduino v0.9.0-rc1

Experimental Windows x64 Boards Manager release for the AT32UC3B0256 with a 12 MHz OSC0 crystal. Install the platform from Boards Manager, then configure the external AVR32 GCC toolchain and dfu-programmer using the [Windows installation guide](https://github.com/basti0772/UC3Bduino/releases/download/v0.9.0-rc1/INSTALL_ARDUINO_WINDOWS.md).

Index URL for Arduino IDE 2.x

`https://raw.githubusercontent.com/basti0772/UC3Bduino/boards-manager/package_uc3bduino_index.json`

Board **UC3Bduino AT32UC3B0256**, FQBN `uc3bduino:avr32:uc3b0256`. Includes GPIO, timing, UART, ADC, native USB CDC, SPI and the existing Wire software master. The red D1 is active-low on PA23. Configure authorized USB application IDs locally to enable CDC. The default upload uses manual factory DFU with image, target, boot-protection and readback checks.

The cumulative firmware baseline is `8619776`. Earlier tests on the revision I reference board covered Blink, protected DFU, USB CDC, automatic upload and internal SPI loopback. Hardware validation is pending for UART, ADC, an external SPI slave, an external I²C device, timing accuracy and long-term USB stability. An intermittent Windows CDC reopening/descriptor failure has been observed. ARDUINO IDE GUI: NOT TESTED.

The project distributes its original sources and retained notices under their respective licenses. AVR32 GCC/G++ 4.3.2 and compatible Binutils are separate prerequisites. The setup helper verifies the original dfu-programmer 1.1.0 upstream Windows x64 archive by size and SHA-256.
