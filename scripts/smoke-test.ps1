# SPDX-License-Identifier: MIT
#Requires -Version 5.1
[CmdletBinding()]
param([string]$SketchbookPath, [string]$CliPath, [string]$ConfigFile,
      [string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
try {
    . (Join-Path $PSScriptRoot 'installation-common.ps1')
    $context = Get-Uc3Context $SketchbookPath $CliPath $ConfigFile
    if (-not (Test-Path -LiteralPath (Join-Path $context.Platform 'platform.local.txt'))) {
        throw "UC3Bduino is not installed in $($context.Sketchbook). Run scripts/install.ps1 first."
    }
    $output = New-Uc3BuildDirectory $OutputDirectory
    Write-Host "Compile only; no upload. Sketchbook: $($context.Sketchbook)"
    Write-Host 'USB compilation does not prove an authorized VID/PID, oscillator or physical USB connection.'
    $examples = Join-Path $context.Platform 'libraries/UC3BduinoExamples/examples'
    foreach ($name in @('Blink','USBSerialBlink')) {
        Invoke-Uc3Compile $context (Join-Path $examples $name) $output
    }
    Write-Host "Smoke tests passed. Artifacts retained in $output"
} catch {
    Write-Error -ErrorAction Continue $_.Exception.Message
    exit 1
}
