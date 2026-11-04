# SPDX-License-Identifier: MIT
#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string]$SketchbookPath = (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino'),
    [string]$ToolchainPath = $env:AVR32_TOOLCHAIN_PATH,
    [string]$DfuToolPath = $env:UC3B_DFU_TOOL_PATH,
    [ValidateSet('Unconfirmed','AT32UC3B0256')]
    [string]$LegacyDevice = 'Unconfirmed',
    [ValidateSet('A', 'B')]
    [string]$GpioPort = 'A',
    [ValidateRange(0, 31)]
    [int]$GpioPin = 23,
    [ValidateSet('U0A', 'U0C', 'U1A', 'U1C', 'U2B', 'U2C')]
    [string]$SerialRoute,
    [ValidateSet(2, 4, 8, 16, 32, 64, 128, 256)]
    [int]$SerialRxBufferSize = 64,
    [ValidateRange(400000, 16000000)]
    [int]$Osc0Hz = 12000000,
    [ValidateSet('External', 'Crystal')]
    [string]$Osc0Mode = 'Crystal',
    [switch]$InternalRc,
    [ValidateRange(1, 65535)]
    [int]$UsbVendorId,
    [ValidateRange(1, 65535)]
    [int]$UsbProductId,
    [switch]$EnableLegacyAutoUpload
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path -Parent $PSScriptRoot

try {
    . (Join-Path $PSScriptRoot 'installation-common.ps1')
    $dfuTool = 'auto'
    # Compile-only installation remains possible without an upload tool.
    if ($DfuToolPath) { $dfuTool = Test-Uc3DfuTool $DfuToolPath }
    $sketchbook = [IO.Path]::GetFullPath($SketchbookPath)
    $destination = Join-Path $sketchbook 'hardware/uc3bduino/avr32'
    if (Test-Path -LiteralPath $destination) {
        throw "Destination already exists: '$destination'. Nothing was changed. Back up/move that package yourself, or select an empty sketchbook path for a fresh install."
    }
    # Do not follow a junction/symlink into an unexpected installation target.
    $ancestor = $destination
    while ($ancestor) {
        if (Test-Path -LiteralPath $ancestor) {
            $item = Get-Item -LiteralPath $ancestor -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Installation path contains a reparse point: '$ancestor'. Choose a direct directory path."
            }
        }
        $ancestor = Split-Path -Parent $ancestor
    }

    $hasPort = $PSBoundParameters.ContainsKey('GpioPort')
    $hasPin = $PSBoundParameters.ContainsKey('GpioPin')
    if ($hasPort -ne $hasPin) {
        throw 'Supply both -GpioPort and -GpioPin, or neither for the reference-board D0=PA23.'
    }
    if ($hasPin -and (($GpioPort -eq 'A' -and $GpioPin -lt 3) -or
                     ($GpioPort -eq 'B' -and $GpioPin -gt 11))) {
        throw 'Choose PA03..PA31 or PB00..PB11 from the board schematic; the example reserves PA00..PA02 for JTAG.'
    }

    $explicitOsc = $PSBoundParameters.ContainsKey('Osc0Hz')
    if ($explicitOsc -ne $PSBoundParameters.ContainsKey('Osc0Mode')) {
        throw 'Supply both -Osc0Hz and -Osc0Mode, or neither for the 12 MHz crystal board profile.'
    }
    if ($InternalRc -and $explicitOsc) { throw 'InternalRc and explicit OSC0 options are mutually exclusive.' }
    $hasOsc = -not $InternalRc
    if ($hasOsc -and ($Osc0Hz % 800 -ne 0)) {
        throw 'OSC0 frequency must be divisible by 800 for the existing 100 Hz timebase.'
    }
    $hasUsb = $PSBoundParameters.ContainsKey('UsbVendorId')
    if ($hasUsb -ne $PSBoundParameters.ContainsKey('UsbProductId')) {
        throw 'Supply both -UsbVendorId and -UsbProductId, using IDs you are authorized to use.'
    }
    if ($hasUsb -and (-not $hasOsc -or $Osc0Hz -notin @(8000000,12000000,16000000))) {
        throw 'USB CDC requires a verified 8, 12 or 16 MHz OSC0 clock and its mode.'
    }
    if ($EnableLegacyAutoUpload -and (-not $hasUsb -or -not $hasOsc -or
        $Osc0Hz -ne 12000000 -or $Osc0Mode -ne 'Crystal' -or $LegacyDevice -ne 'AT32UC3B0256')) {
        throw 'Legacy auto-upload requires explicit USB IDs, confirmed AT32UC3B0256 and a 12 MHz crystal. Verify factory DFU 1.0.2 and accept nonvolatile ISP_FORCE (GP31); see docs/AUTO_UPLOAD.md.'
    }
    if ($hasOsc -and $SerialRoute -eq 'U0A') {
        throw 'U0A RX uses PA18/XIN0 and conflicts with OSC0. Select another verified route.'
    }
    $routes = @('U0A', 'U0C', 'U1A', 'U1C', 'U2B', 'U2C')
    if ($SerialRoute) { $SerialRoute = $SerialRoute.ToUpperInvariant() }
    $routePins = @{ U0A = @(18,19); U0C = @(42,43); U1A = @(24,23); U1C = @(35,34); U2B = @(27,26); U2C = @(20,21) }
    if ($SerialRoute -eq 'U1A') { throw 'U1A TX uses the onboard LED pad PA23. Select another route.' }
    $gpioNumber = $GpioPin + $(if ($GpioPort -eq 'B') { 32 } else { 0 })
    if ($SerialRoute -and $gpioNumber -in $routePins[$SerialRoute]) {
        throw 'D0 conflicts with the selected Serial TX/RX route.'
    }
    if ($hasOsc -and ($gpioNumber -eq 18 -or ($Osc0Mode -eq 'Crystal' -and $gpioNumber -eq 19))) {
        throw 'D0 conflicts with an OSC0 pad.'
    }

    $toolchain = Resolve-Uc3Toolchain $ToolchainPath
    $binDirectory = $toolchain.Bin
    Write-Host $toolchain.Version

    $template = Join-Path $repoRoot 'arduino/hardware/uc3bduino/avr32'
    $shared = @(
        @{ Source = 'src/hal/uc3b_i2c.c'; Target = 'cores/uc3bduino/hal/uc3b_i2c.c' },
        @{ Source = 'src/hal/uc3b_i2c.h'; Target = 'cores/uc3bduino/hal/uc3b_i2c.h' },
        @{ Source = 'src/hal/uc3b_i2c_hw.h'; Target = 'cores/uc3bduino/hal/uc3b_i2c_hw.h' },
        @{ Source = 'src/hal/uc3b_spi.c'; Target = 'cores/uc3bduino/hal/uc3b_spi.c' },
        @{ Source = 'src/hal/uc3b_spi.h'; Target = 'cores/uc3bduino/hal/uc3b_spi.h' },
        @{ Source = 'src/hal/uc3b_spi_hw.h'; Target = 'cores/uc3bduino/hal/uc3b_spi_hw.h' },
        @{ Source = 'tools/AutoUpload.ps1'; Target = 'tools/AutoUpload.ps1' },
        @{ Source = 'tools/auto-upload-common.ps1'; Target = 'tools/auto-upload-common.ps1' },
        @{ Source = 'src/hal/uc3b_bootloader.c'; Target = 'cores/uc3bduino/hal/uc3b_bootloader.c' },
        @{ Source = 'src/hal/uc3b_bootloader.h'; Target = 'cores/uc3bduino/hal/uc3b_bootloader.h' },
        @{ Source = 'src/hal/uc3b_bootloader_hw.h'; Target = 'cores/uc3bduino/hal/uc3b_bootloader_hw.h' },
        @{ Source = 'src/usb/usb_boot_touch.c'; Target = 'cores/uc3bduino/usb/usb_boot_touch.c' },
        @{ Source = 'src/usb/usb_boot_touch.h'; Target = 'cores/uc3bduino/usb/usb_boot_touch.h' },
        @{ Source = 'src/hal/uc3b_adc.c'; Target = 'cores/uc3bduino/hal/uc3b_adc.c' },
        @{ Source = 'src/hal/uc3b_adc.h'; Target = 'cores/uc3bduino/hal/uc3b_adc.h' },
        @{ Source = 'src/hal/uc3b_adc_hw.h'; Target = 'cores/uc3bduino/hal/uc3b_adc_hw.h' },
        @{ Source = 'src/hal/uc3b_clock.c'; Target = 'cores/uc3bduino/hal/uc3b_clock.c' },
        @{ Source = 'src/hal/uc3b_clock.h'; Target = 'cores/uc3bduino/hal/uc3b_clock.h' },
        @{ Source = 'src/hal/uc3b_gpio.c'; Target = 'cores/uc3bduino/hal/uc3b_gpio.c' },
        @{ Source = 'src/hal/uc3b_gpio.h'; Target = 'cores/uc3bduino/hal/uc3b_gpio.h' },
        @{ Source = 'src/hal/uc3b_device.h'; Target = 'cores/uc3bduino/hal/uc3b_device.h' },
        @{ Source = 'src/hal/uc3b_system_timer.c'; Target = 'cores/uc3bduino/hal/uc3b_system_timer.c' },
        @{ Source = 'src/hal/uc3b_system_timer.h'; Target = 'cores/uc3bduino/hal/uc3b_system_timer.h' },
        @{ Source = 'src/hal/uc3b_system_timer_hw.h'; Target = 'cores/uc3bduino/hal/uc3b_system_timer_hw.h' },
        @{ Source = 'src/hal/uc3b_irq.h'; Target = 'cores/uc3bduino/hal/uc3b_irq.h' },
        @{ Source = 'src/hal/uc3b_uart.h'; Target = 'cores/uc3bduino/hal/uc3b_uart.h' },
        @{ Source = 'src/hal/uc3b_uart.c'; Target = 'cores/uc3bduino/hal/uc3b_uart.c' },
        @{ Source = 'src/hal/uc3b_uart_hw.h'; Target = 'cores/uc3bduino/hal/uc3b_uart_hw.h' },
        @{ Source = 'src/usb/usb_config.h'; Target = 'cores/uc3bduino/usb/usb_config.h' },
        @{ Source = 'src/usb/uc3b_usb_device.c'; Target = 'cores/uc3bduino/usb/uc3b_usb_device.c' },
        @{ Source = 'src/usb/uc3b_usb_device.h'; Target = 'cores/uc3bduino/usb/uc3b_usb_device.h' },
        @{ Source = 'src/usb/usb_device_core.c'; Target = 'cores/uc3bduino/usb/usb_device_core.c' },
        @{ Source = 'src/usb/usb_device_core.h'; Target = 'cores/uc3bduino/usb/usb_device_core.h' },
        @{ Source = 'src/usb/usb_cdc.c'; Target = 'cores/uc3bduino/usb/usb_cdc.c' },
        @{ Source = 'src/usb/usb_cdc.h'; Target = 'cores/uc3bduino/usb/usb_cdc.h' },
        @{ Source = 'src/usb/usb_fifo.h'; Target = 'cores/uc3bduino/usb/usb_fifo.h' },
        @{ Source = 'docs/USB_CDC.md'; Target = 'docs/USB_CDC.md' },
        @{ Source = 'src/startup/uc3_startup.S'; Target = 'cores/uc3bduino/startup/uc3_startup.S' },
        @{ Source = 'linker/at32uc3b0256.ld'; Target = 'system/at32uc3b0256.ld' },
        @{ Source = 'LICENSE'; Target = 'LICENSE' },
        @{ Source = 'docs/THIRD_PARTY.md'; Target = 'docs/THIRD_PARTY.md' },
        @{ Source = 'docs/THIRD_PARTY_NOTICES.md'; Target = 'docs/THIRD_PARTY_NOTICES.md' },
        @{ Source = 'docs/DEVICE_HEADERS.md'; Target = 'docs/DEVICE_HEADERS.md' },
        @{ Source = 'scripts/upload-dfu.ps1'; Target = 'tools/upload-dfu.ps1' },
        @{ Source = 'scripts/dfu-common.ps1'; Target = 'tools/dfu-common.ps1' },
        @{ Source = 'scripts/dfu-usb.cs'; Target = 'tools/dfu-usb.cs' },
        @{ Source = 'docs/DFU_UPLOAD.md'; Target = 'docs/DFU_UPLOAD.md' }
    )
    # Installed examples remain copies of the canonical sketches, like the HAL.
    foreach ($example in Get-ChildItem -LiteralPath (Join-Path $repoRoot 'examples') -Directory) {
        if (Test-Path -LiteralPath (Join-Path $example.FullName ($example.Name + '.ino'))) {
            foreach ($file in Get-ChildItem -LiteralPath $example.FullName -File) {
                if ($file.Extension -in @('.ino', '.md')) {
                    $shared += @{ Source = "examples/$($example.Name)/$($file.Name)"; Target = "libraries/UC3BduinoExamples/examples/$($example.Name)/$($file.Name)" }
                }
            }
        }
    }
    foreach ($doc in Get-ChildItem -LiteralPath (Join-Path $repoRoot 'docs') -Filter '*.md' -File) {
        $relative = "docs/$($doc.Name)"
        if ($relative -notin $shared.Target) {
            $shared += @{ Source = $relative; Target = $relative }
        }
    }
    $shared += @{ Source = 'VERSION'; Target = 'VERSION' }
    $shared += @{ Source = 'CHANGELOG.md'; Target = 'CHANGELOG.md' }
    foreach ($entry in $shared) {
        if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $entry.Source) -PathType Leaf)) {
            throw "Missing shared source: $($entry.Source). Install from a complete UC3Bduino checkout."
        }
    }

    if ($PSCmdlet.ShouldProcess($destination, 'Install local UC3Bduino build and DFU upload package')) {
        New-Item -ItemType Directory -Path $destination -Force | Out-Null
        Get-ChildItem -LiteralPath $template | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $destination -Recurse
        }
        $manifest = @()
        foreach ($entry in $shared) {
            $source = Join-Path $repoRoot $entry.Source
            $target = Join-Path $destination $entry.Target
            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
            Copy-Item -LiteralPath $source -Destination $target
            $manifest += @{ source = $entry.Source; installed = $entry.Target; sha256 = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash }
        }
        $utf8 = New-Object Text.UTF8Encoding($false)
        $localConfig = "# Generated local path; re-run installation after moving the toolchain.`ncompiler.path=$($binDirectory.Replace('\','/'))/`n"
        $localConfig += "tools.uc3bdfu.path=$($dfuTool.Replace('\','/'))`ntools.uc3bdfu.legacy_device=$LegacyDevice`n"
        if ($EnableLegacyAutoUpload) {
            # Let this script own the touch; Arduino must not perform a second one.
            $localConfig += 'tools.uc3bdfu.upload.pattern=powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "{runtime.platform.path}/tools/AutoUpload.ps1" -Port "{serial.port}" -Firmware "{build.path}/{build.project_name}.hex" -ToolPath "{tools.uc3bdfu.path}" -LegacyDevice "{tools.uc3bdfu.legacy_device}"' + "`n"
        }
        [IO.File]::WriteAllText((Join-Path $destination 'platform.local.txt'), $localConfig, $utf8)
        $buildFlags = "-DSERIAL_RX_BUFFER_SIZE=$SerialRxBufferSize"
        if (-not $hasOsc) { $buildFlags += " -DUC3B_CLOCK_SOURCE=1" }
        if ($hasUsb) {
            $buildFlags += " -DUC3BDUINO_USB_VENDOR_ID=$UsbVendorId -DUC3BDUINO_USB_PRODUCT_ID=$UsbProductId"
        }
        if ($EnableLegacyAutoUpload) {
            $buildFlags += ' -DUC3BDUINO_USB_AUTO_UPLOAD=1 -DUC3BDUINO_FACTORY_DFU_GP31=1'
            Write-Warning 'Legacy auto-upload enabled: a 1200-baud DTR close changes nonvolatile GP31 before watchdog reset. Factory DFU 1.0.2 must be verified. See docs/AUTO_UPLOAD.md.'
        }
        $boardConfig = "# Generated explicit board options. Verify hardware before flashing.`n"
        if ($hasOsc) {
            $crystal = if ($Osc0Mode -eq 'Crystal') { 1 } else { 0 }
            $buildFlags += " -DUC3B_CLOCK_SOURCE=2 -DUC3B_OSC0_HZ=$($Osc0Hz)UL -DUC3B_OSC0_CRYSTAL=$crystal"
            $boardConfig += "uc3b0256.build.f_cpu=$($Osc0Hz)L`n"
        } else {
            $boardConfig += "uc3b0256.build.f_cpu=115200L`n"
        }
        $boardConfig += "uc3b0256.build.extra_flags=$buildFlags`n"
        if ($EnableLegacyAutoUpload) {
            $boardConfig += "uc3b0256.upload.use_1200bps_touch=false`nuc3b0256.upload.wait_for_upload_port=false`n"
        }
        [IO.File]::WriteAllText((Join-Path $destination 'boards.local.txt'), $boardConfig, $utf8)
        if ($SerialRoute) {
            $routeIndex = [Array]::IndexOf($routes, $SerialRoute)
            $serialConfig = "/* Explicit route $SerialRoute; verify board wiring before flashing. */`n#pragma once`n#define UC3BDUINO_SERIAL_ROUTE $routeIndex`n"
            [IO.File]::WriteAllText((Join-Path $destination 'variants/uc3b0256/serial_config.h'), $serialConfig, $utf8)
        }
        if ($hasPin) {
            $portIndex = if ($GpioPort -eq 'A') { 0 } else { 1 }
            $pinConfig = "/* Generated explicit test-pin selection. Verify board wiring before flashing. */`n#pragma once`n#define UC3BDUINO_TEST_PIN_CONFIGURED 1`n#define UC3BDUINO_TEST_PIN_PORT $portIndex`n#define UC3BDUINO_TEST_PIN_BIT $GpioPin`n"
            [IO.File]::WriteAllText((Join-Path $destination 'variants/uc3b0256/test_pin_config.h'), $pinConfig, $utf8)
        }
        [IO.File]::WriteAllText((Join-Path $destination 'shared-sources.json'), ($manifest | ConvertTo-Json -Depth 3), $utf8)
        Write-Host "Installed: $destination"
        Write-Host 'FQBN: uc3bduino:avr32:uc3b0256. Restart Arduino IDE after installation.'
        if (-not $hasPin) { Write-Host 'Reference board: D0=PA23; LED_BUILTIN is fixed to red D1/PA23, active low.' }
        if (-not $SerialRoute) { Write-Host 'Serial is unconfigured; begin() leaves it inactive until a verified route is selected.' }
        if (-not $hasOsc) { Write-Host 'Internal RC selected. 115200 baud requires an explicitly configured faster clock.' }
        if (-not $hasUsb) { Write-Host 'USB VID/PID must be configured by the developer. SerialUSB stays detached until configured.' }
        Write-Host 'Factory DFU requires its own supported OSC0 hardware (8/12/16 MHz crystal), even for RC-clocked sketches.'
        if ($dfuTool -eq 'auto') { Write-Host 'DFU tool path not recorded; uploads resolve UC3B_DFU_TOOL_PATH or PATH in the IDE process.' }
    }
} catch {
    Write-Error -ErrorAction Continue "Arduino hardware installation failed: $($_.Exception.Message)"
    exit 1
}
