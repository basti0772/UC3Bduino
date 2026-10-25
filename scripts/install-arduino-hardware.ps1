# SPDX-License-Identifier: MIT
#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter(Mandatory = $true)]
    [string]$SketchbookPath,
    [string]$ToolchainPath = $env:AVR32_TOOLCHAIN_PATH,
    [ValidateSet('A', 'B')]
    [string]$GpioPort,
    [ValidateRange(0, 31)]
    [int]$GpioPin
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path -Parent $PSScriptRoot

try {
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
        @{ Source = 'src/startup/uc3_startup.S'; Target = 'cores/uc3bduino/startup/uc3_startup.S' },
        @{ Source = 'linker/at32uc3b0256.ld'; Target = 'system/at32uc3b0256.ld' },
        @{ Source = 'LICENSE'; Target = 'LICENSE' },
        @{ Source = 'docs/THIRD_PARTY.md'; Target = 'docs/THIRD_PARTY.md' },
        @{ Source = 'docs/THIRD_PARTY_NOTICES.md'; Target = 'docs/THIRD_PARTY_NOTICES.md' },
        @{ Source = 'docs/DEVICE_HEADERS.md'; Target = 'docs/DEVICE_HEADERS.md' }
    )
    foreach ($entry in $shared) {
        if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $entry.Source) -PathType Leaf)) {
            throw "Missing shared source: $($entry.Source). Install from a complete UC3Bduino checkout."
        }
    }

    if ($PSCmdlet.ShouldProcess($destination, 'Install local UC3Bduino compilation package')) {
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
        [IO.File]::WriteAllText((Join-Path $destination 'platform.local.txt'), $localConfig, $utf8)
        if ($hasPin) {
            $portIndex = if ($GpioPort -eq 'A') { 0 } else { 1 }
            $pinConfig = "/* Generated explicit test-pin selection. Verify board wiring before flashing. */`n#pragma once`n#define UC3BDUINO_TEST_PIN_CONFIGURED 1`n#define UC3BDUINO_TEST_PIN_PORT $portIndex`n#define UC3BDUINO_TEST_PIN_BIT $GpioPin`n"
            [IO.File]::WriteAllText((Join-Path $destination 'variants/uc3b0256/test_pin_config.h'), $pinConfig, $utf8)
        }
        [IO.File]::WriteAllText((Join-Path $destination 'shared-sources.json'), ($manifest | ConvertTo-Json -Depth 3), $utf8)
        Write-Host "Installed: $destination"
        Write-Host 'FQBN: uc3bduino:avr32:uc3b0256. Restart Arduino IDE after installation.'
        if (-not $hasPin) { Write-Host 'D0 is unconfigured. ArduinoMinimal builds; DigitalOutput requires an explicit pin mapping.' }
    }
} catch {
    Write-Error -ErrorAction Continue "Arduino hardware installation failed: $($_.Exception.Message)"
    exit 1
}
