# UC3Bduino 0.9.0-rc1 unter Windows 11 x64

Der Boards Manager installiert die Plattform für den AT32UC3B0256 mit 12-MHz-Quarz. Der AVR32-Compiler wird separat installiert. Das einmalige Setup in Schritt 4 richtet auch dfu-programmer 1.1.0 ein.

1. Installiere [Arduino IDE 2.x](https://www.arduino.cc/en/software/). Öffne **Datei → Voreinstellungen → Zusätzliche Boardverwalter-URLs** und füge folgende URL ein.

   `https://raw.githubusercontent.com/basti0772/UC3Bduino/boards-manager/package_uc3bduino_index.json`

2. Suche im Boardverwalter nach **UC3Bduino**, wähle **0.9.0-rc1** und klicke auf **Installieren**. Wähle unter **Werkzeuge → Board → UC3Bduino** das Board **UC3Bduino AT32UC3B0256**. Der FQBN lautet `uc3bduino:avr32:uc3b0256`. Bei einer früheren Sketchbook-Installation verschiebe deren Verzeichnis `hardware/uc3bduino/avr32` zur Sicherung aus dem Sketchbook und starte die IDE neu. So verwendet die IDE den Boards-Manager-Core.

3. Installiere die vollständige **AVR32 GNU Toolchain für Windows** mit Geräteheadern und Binutils. Getestet ist AVR32 GCC/G++ 4.3.2 mit Binutils 2.19.20081016. Microchip führt den [Windows-Installer 2.4.2](https://ww1.microchip.com/downloads/archive/avr32-gnu-toolchain-2.4.2-setup.exe) im [Herstellerarchiv](https://www.microchip.com/en-us/tools-resources/archives/avr-sam-mcus). Die exakte Herkunft des hier geprüften Legacy-Installers ist offen. Daher wird die Toolchain extern bezogen. Für den RC ist die Prüfung der lokalen Installation unten maßgeblich.

4. Schließe die IDE. Führe die folgenden Befehle in PowerShell aus. Bei abweichendem Arduino-Datenverzeichnis passe die erste Zeile an. Das Skript erkennt den Compiler über die Variable `AVR32_TOOLCHAIN_PATH`, den Suchpfad oder den Atmel-Standardordner. Bei eigener Installation übergib zusätzlich `-ToolchainPath` mit dem vollständigen Pfad zum installierten AVR32-Verzeichnis. Die Einrichtung prüft gcc/g++, ucr1 und die Binutils und speichert die ermittelten Pfade für diese Plattformversion. Für DFU erkennt es UC3B_DFU_TOOL_PATH oder den Suchpfad. Beim ersten Start ohne vorhandenes Werkzeug lädt es das unveränderte Windows-x64-Archiv von dfu-programmer 1.1.0 vom Hersteller, prüft Größe und SHA-256 und entpackt es unter %LOCALAPPDATA%/UC3Bduino/tools. Eine vorhandene Installation lässt sich mit -DfuToolPath auswählen.

   ```powershell
   $platform = Join-Path $env:LOCALAPPDATA 'Arduino15/packages/uc3bduino/hardware/avr32/0.9.0-rc1'
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$platform/tools/setup-boards-manager.ps1"
   ```

5. Öffne die IDE erneut. Unter **Datei → Beispiele → UC3BduinoExamples → Blink** klicke auf **Überprüfen**. In der ausführlichen Compiler-Ausgabe muss der Plattformpfad auf `packages/uc3bduino/hardware/avr32/0.9.0-rc1` enden.

6. Prüfe die Chipmarkierung **AT32UC3B0256**. Beim bestätigten Referenzboard mit älterer Factory-DFU-Signatur wähle **Werkzeuge → DFU target identification → Confirmed AT32UC3B0256 marking (legacy DFU)**. Bei abweichender oder unklarer Markierung bleibt die automatische Signaturprüfung aktiv.

7. Aktiviere Factory-DFU mit dem Boot-/Reset-Verfahren des Boards. Bei der originalen B0/B1-Konfiguration halte **PA13** beim externen Reset auf LOW, gib RESET_N frei, warte auf die USB-Erkennung und lasse PA13 los. Eine geänderte Bootloaderkonfiguration kann einen anderen Eingang verwenden. Kontrolliere im Geräte-Manager die Hardware-ID **USB VID_03EB & PID_2FF6**. Falls der DFU-Treiber fehlt, wähle in [Zadig](https://zadig.akeo.ie/) unter **Options → List All Devices** ausschließlich dieses DFU-Gerät und installiere **WinUSB**. Der CDC-Port der Anwendung verwendet den seriellen Windows-Treiber.

8. Klicke auf **Hochladen**. Der manuell aktivierte Bootloader benötigt keinen COM-Port. Der Uploader prüft Firmwarebereich, Gerät, Bootschutz und Readback vor dem Anwendungsstart. Blink schaltet die rote D1 an **PA23** je 500 ms an und aus. LOW schaltet sie ein. Bei Fehlern kontrolliere USB-Datenkabel, DFU-ID und WinUSB. Bei einem Schutz- oder Identitätsfehler kontrolliere die Hardware und den Bootloader. [DFU-Details](DFU_UPLOAD.md)

9. Optional für **USBSerialBlink** oder **USBSerialEcho**: Die CDC-Anwendung meldet sich bei den Vorgabewerten VID=0/PID=0 absichtlich nicht an. Verwende ein dir zugewiesenes oder ausdrücklich lizenziertes Paar. Führe das Setup mit beiden Werten erneut aus und starte die IDE neu. Danach Beispiel kompilieren, erneut in Factory-DFU hochladen und den neu erschienenen CDC-Port im seriellen Monitor auswählen. Beim erneuten Setup ohne IDs gelten wieder die öffentlichen Vorgabewerte.

   ```powershell
   $vid = [Convert]::ToInt32((Read-Host 'Zugelassene VID in Hex'), 16)
   $pidValue = [Convert]::ToInt32((Read-Host 'Zugelassene PID in Hex'), 16)
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$platform/tools/setup-boards-manager.ps1" -UsbVendorId $vid -UsbProductId $pidValue
   ```

Der native USB-Takt entsteht aus dem 12-MHz-Quarz über PLL1 und GCLK3. Ein DFU-Upload darf nur den Anwendungsbereich ab 0x80002000 programmieren. Bei Downloadfehlern prüfe die GitHub-Erreichbarkeit und vergleiche das heruntergeladene Archiv mit SHA256SUMS.txt im Release. Der Setup-Download enthält die benötigten DLLs und die Lizenzdateien.
