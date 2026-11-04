# SPDX-License-Identifier: MIT
#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string]$SketchbookPath,
    [string]$CliPath,
    [string]$ConfigFile,
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
try {
    . (Join-Path $PSScriptRoot 'installation-common.ps1')
    $context = Get-Uc3Context $SketchbookPath $CliPath $ConfigFile
    Write-Host "Sketchbook: $($context.Sketchbook) [$($context.Source)]"
    $forward = @{}
    foreach ($key in $PSBoundParameters.Keys) {
        if ($key -notin @('CliPath','ConfigFile','SketchbookPath')) { $forward[$key] = $PSBoundParameters[$key] }
    }
    $forward.SketchbookPath = $context.Sketchbook
    # Record a validated executable, so IDE restarts do not depend on shell PATH.
    # An explicitly requested tool must work. Automatic absence permits compilation.
    if ($DfuToolPath) {
        $forward.DfuToolPath = Test-Uc3DfuTool $DfuToolPath
    } else {
        $dfuCommand = Get-Command dfu-programmer.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($dfuCommand) { $forward.DfuToolPath = Test-Uc3DfuTool $dfuCommand.Source }
        else { Write-Warning 'DFU tool not found: compile-only installation. See docs/INSTALL_WINDOWS.md for the pinned 1.1.0 download and checksum.' }
    }
    & (Join-Path $PSScriptRoot 'install-arduino-hardware.ps1') @forward
    if (-not $?) { throw 'Platform installation failed.' }
    if (-not $WhatIfPreference) {
        Write-Host 'Next: restart Arduino IDE, select UC3Bduino AT32UC3B0256, then File > Examples > UC3BduinoExamples > Blink.'
        Write-Host 'Run scripts/diagnose.ps1 and scripts/smoke-test.ps1 with the same -SketchbookPath. Neither script uploads firmware.'
    }
} catch {
    Write-Error -ErrorAction Continue "Installation failed: $($_.Exception.Message)"
    exit 1
}
