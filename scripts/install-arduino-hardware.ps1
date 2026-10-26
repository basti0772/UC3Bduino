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
    [string]$GpioPort,
    [ValidateRange(0, 31)]
    [int]$GpioPin,
    [ValidateSet('U0A', 'U0C', 'U1A', 'U1C', 'U2B', 'U2C')]
    [string]$SerialRoute,
    [ValidateSet(2, 4, 8, 16, 32, 64, 128, 256)]
    [int]$SerialRxBufferSize = 64,
    [ValidateRange(400000, 16000000)]
    [int]$Osc0Hz,
    [ValidateSet('External', 'Crystal')]
    [string]$Osc0Mode
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path -Parent $PSScriptRoot

try {
    . (Join-Path $PSScriptRoot 'dfu-common.ps1')
    $dfuTool = 'auto'
    # Compile-only installation remains possible without an upload tool.
    if ($DfuToolPath) { $dfuTool = Resolve-DfuTool $DfuToolPath }
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
        throw 'Supply both -GpioPort and -GpioPin, or neither for an unconfigured D0.'
    }
    if ($hasPin -and (($GpioPort -eq 'A' -and $GpioPin -lt 3) -or
                     ($GpioPort -eq 'B' -and $GpioPin -gt 11))) {
        throw 'Choose PA03..PA31 or PB00..PB11 from the board schematic; the example reserves PA00..PA02 for JTAG.'
    }

    $hasOsc = $PSBoundParameters.ContainsKey('Osc0Hz')
    if ($hasOsc -ne $PSBoundParameters.ContainsKey('Osc0Mode')) {
        throw 'Supply both -Osc0Hz and -Osc0Mode, or neither for internal RC.'
    }
    if ($hasOsc -and ($Osc0Hz % 800 -ne 0)) {
        throw 'OSC0 frequency must be divisible by 800 for the existing 100 Hz timebase.'
    }
    if ($hasOsc -and $SerialRoute -eq 'U0A') {
        throw 'U0A RX uses PA18/XIN0 and conflicts with OSC0. Select another verified route.'
    }
    $routes = @('U0A', 'U0C', 'U1A', 'U1C', 'U2B', 'U2C')
    if ($SerialRoute) { $SerialRoute = $SerialRoute.ToUpperInvariant() }
    $routePins = @{ U0A = @(18,19); U0C = @(42,43); U1A = @(24,23); U1C = @(35,34); U2B = @(27,26); U2C = @(20,21) }
    if ($hasPin) {
        $gpioNumber = $GpioPin + $(if ($GpioPort -eq 'B') { 32 } else { 0 })
        if ($SerialRoute -and $gpioNumber -in $routePins[$SerialRoute]) {
            throw 'D0 conflicts with the selected Serial TX/RX route.'
        }
        if ($hasOsc -and ($gpioNumber -eq 18 -or ($Osc0Mode -eq 'Crystal' -and $gpioNumber -eq 19))) {
            throw 'D0 conflicts with an OSC0 pad.'
        }
    }

    if ($ToolchainPath) {
        $candidate = [IO.Path]::GetFullPath($ToolchainPath)
        if (Test-Path -LiteralPath (Join-Path $candidate 'bin/avr32-gcc.exe')) {
            $binDirectory = Join-Path $candidate 'bin'
        } elseif (Test-Path -LiteralPath (Join-Path $candidate 'avr32-gcc.exe')) {
            $binDirectory = $candidate
        } else {
            throw "No avr32-gcc.exe in '$ToolchainPath' or its bin directory."
        }
    } else {
        $gcc = Get-Command avr32-gcc.exe -CommandType Application -ErrorAction SilentlyContinue
        if (-not $gcc) { throw 'Set AVR32_TOOLCHAIN_PATH or pass -ToolchainPath (legacy AVR32 GNU root or bin).' }
        $binDirectory = Split-Path -Parent $gcc.Source
    }
    foreach ($tool in @('gcc', 'g++', 'as', 'ld', 'ar', 'objcopy', 'size')) {
        if (-not (Test-Path -LiteralPath (Join-Path $binDirectory "avr32-$tool.exe") -PathType Leaf)) {
            throw "Missing avr32-$tool.exe in '$binDirectory'. Arduino sketches require the C++ compiler too."
        }
    }

    $template = Join-Path $repoRoot 'arduino/hardware/uc3bduino/avr32'
    $shared = @(
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
        @{ Source = 'src/startup/uc3_startup.S'; Target = 'cores/uc3bduino/startup/uc3_startup.S' },
        @{ Source = 'linker/at32uc3b0256.ld'; Target = 'system/at32uc3b0256.ld' },
        @{ Source = 'LICENSE'; Target = 'LICENSE' },
        @{ Source = 'docs/THIRD_PARTY.md'; Target = 'docs/THIRD_PARTY.md' },
        @{ Source = 'docs/THIRD_PARTY_NOTICES.md'; Target = 'docs/THIRD_PARTY_NOTICES.md' },
        @{ Source = 'docs/DEVICE_HEADERS.md'; Target = 'docs/DEVICE_HEADERS.md' },
        @{ Source = 'scripts/upload-dfu.ps1'; Target = 'tools/upload-dfu.ps1' },
        @{ Source = 'scripts/dfu-common.ps1'; Target = 'tools/dfu-common.ps1' },
        @{ Source = 'docs/DFU_UPLOAD.md'; Target = 'docs/DFU_UPLOAD.md' }
    )
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
        [IO.File]::WriteAllText((Join-Path $destination 'platform.local.txt'), $localConfig, $utf8)
        $buildFlags = "-DSERIAL_RX_BUFFER_SIZE=$SerialRxBufferSize"
        $boardConfig = "# Generated explicit board options. Verify hardware before flashing.`n"
        if ($hasOsc) {
            $crystal = if ($Osc0Mode -eq 'Crystal') { 1 } else { 0 }
            $buildFlags += " -DUC3B_CLOCK_SOURCE=2 -DUC3B_OSC0_HZ=$($Osc0Hz)UL -DUC3B_OSC0_CRYSTAL=$crystal"
            $boardConfig += "uc3b0256.build.f_cpu=$($Osc0Hz)L`n"
        }
        $boardConfig += "uc3b0256.build.extra_flags=$buildFlags`n"
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
        if (-not $hasPin) { Write-Host 'D0 is unconfigured. ArduinoMinimal builds; DigitalOutput requires an explicit pin mapping.' }
        if (-not $SerialRoute) { Write-Host 'Serial is unconfigured; begin() leaves it inactive until a verified route is selected.' }
        if (-not $hasOsc) { Write-Host 'Internal RC selected. 115200 baud requires an explicitly configured faster clock.' }
        Write-Host 'Factory DFU requires its own supported OSC0 hardware (8/12/16 MHz crystal), even for RC-clocked sketches.'
        if ($dfuTool -eq 'auto') { Write-Host 'DFU tool path not recorded; uploads resolve UC3B_DFU_TOOL_PATH or PATH in the IDE process.' }
    }
} catch {
    Write-Error -ErrorAction Continue "Arduino hardware installation failed: $($_.Exception.Message)"
    exit 1
}
