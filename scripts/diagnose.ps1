# SPDX-License-Identifier: MIT
#Requires -Version 5.1
[CmdletBinding()]
param([string]$SketchbookPath, [string]$CliPath, [string]$ConfigFile,
      [string]$ToolchainPath, [string]$DfuToolPath, [string]$OutputDirectory,
      [switch]$SkipCompile)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'installation-common.ps1')
$failed = $false
function Show-Check([string]$Name, [scriptblock]$Check) {
    try { $detail = & $Check; Write-Host "${Name}: PASS $detail" }
    catch { Write-Host "${Name}: FAIL $($_.Exception.Message)"; $script:failed = $true }
}
try {
    $context = Get-Uc3Context $SketchbookPath $CliPath $ConfigFile
} catch { Write-Error -ErrorAction Continue $_.Exception.Message; exit 1 }
Write-Host "Sketchbook: $($context.Sketchbook) [$($context.Source)]"
Write-Host 'Diagnostics never upload or send DFU protocol commands. PnP presence does not verify the driver or exact MCU.'
Write-Host "Windows: $([Environment]::OSVersion.VersionString) / PowerShell $($PSVersionTable.PSVersion)"
Show-Check 'Arduino CLI' {
    if (-not $context.Cli) { throw 'Install Arduino CLI or pass -CliPath (also accepts the IDE-bundled CLI).' }
    (Invoke-Uc3Process $context.Cli @('version')).Output.Trim()
}
$ide = @("$env:ProgramFiles/Arduino IDE/Arduino IDE.exe", "$env:LOCALAPPDATA/Programs/Arduino IDE/Arduino IDE.exe") |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if ($ide) { Write-Host "Arduino IDE: FOUND $ide (GUI not tested)" }
else { Write-Host 'Arduino IDE: NOT FOUND in standard locations; CLI use is supported. IDE 2.x: https://www.arduino.cc/en/software/' }
$local = @{}
Show-Check 'UC3Bduino platform' {
    $properties = Read-Uc3Properties (Join-Path $context.Platform 'platform.txt')
    $script:local = Read-Uc3Properties (Join-Path $context.Platform 'platform.local.txt')
    "$($properties['version']) at $($context.Platform)"
}
Show-Check 'AVR32 GCC' {
    $selected = $ToolchainPath
    if (-not $selected -and $local.ContainsKey('compiler.path')) { $selected = $local['compiler.path'] }
    $tc = Resolve-Uc3Toolchain $selected
    if ($local.ContainsKey('compiler.path') -and
        [IO.Path]::GetFullPath($local['compiler.path']).TrimEnd('/','\') -ne $tc.Bin.TrimEnd('/','\')) {
        throw 'Requested toolchain differs from installed compiler.path. Reinstall into a fresh sketchbook to change it.'
    }
    "$($tc.Version) [$($tc.Bin)]"
}
$selectedDfu = $DfuToolPath
if (-not $selectedDfu -and $local.ContainsKey('tools.uc3bdfu.path')) { $selectedDfu = $local['tools.uc3bdfu.path'] }
Show-Check 'DFU tool' { Test-Uc3DfuTool $selectedDfu }
Show-Check 'Board FQBN' {
    if (-not $context.Cli) { throw 'Arduino CLI is unavailable.' }
    $boards = Invoke-Uc3Process $context.Cli ($context.CliArgs + @('board','listall','UC3Bduino')) $context.Environment
    if ($boards.Output -notmatch 'uc3bduino:avr32:uc3b0256') { throw 'Board not visible to this CLI configuration.' }
    'uc3bduino:avr32:uc3b0256'
}
if ($SkipCompile) { Write-Host 'FQBN compile: SKIPPED (-SkipCompile)' }
else {
    Show-Check 'FQBN compile' {
        $output = New-Uc3BuildDirectory $OutputDirectory
        Invoke-Uc3Compile $context (Join-Path $context.Platform 'libraries/UC3BduinoExamples/examples/ArduinoMinimal') $output
        "ArduinoMinimal; artifacts: $output"
    }
}
try {
    # Deliberately no Get-DfuTarget / ProbeOnly: even Atmel read queries may set ISP_FORCE.
    $devices = @(Get-PnpDevice -PresentOnly -ErrorAction Stop | Where-Object {
        $_.InstanceId -match '^USB\\VID_03EB&PID_2FF6(?:&REV_[0-9A-F]{4})?\\'
    })
    if ($devices.Count -eq 0) { Write-Host 'DFU device: NOT CONNECTED (03EB:2FF6)' }
    elseif ($devices.Count -eq 1) { Write-Host "DFU device: PRESENT (PnP status: $($devices[0].Status)); driver access and MCU marking still need verification." }
    else { Write-Host "DFU device: MULTIPLE ($($devices.Count)); connect only one before explicit upload." }
} catch { Write-Host "DFU device: UNKNOWN (PnP unavailable: $($_.Exception.Message))" }
if ($failed) { exit 1 }
