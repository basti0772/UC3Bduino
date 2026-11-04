# SPDX-License-Identifier: MIT
#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$SketchbookPath, [string]$CliPath, [string]$ConfigFile,
    [string]$OutputDirectory, [string]$Port,
    [ValidateRange(0,65535)][int]$UsbVendorId,
    [ValidateRange(0,65535)][int]$UsbProductId,
    [string]$ToolPath,
    [ValidateSet('Unconfirmed','AT32UC3B0256')][string]$LegacyDevice = 'Unconfirmed',
    [ValidateRange(1,120)][int]$DfuTimeoutSeconds = 20,
    [ValidateRange(1,120)][int]$ApplicationTimeoutSeconds = 30,
    [ValidateRange(1,120)][int]$IdentityTimeoutSeconds = 15,
    [switch]$BuildOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
try {
    $root = Split-Path -Parent $PSScriptRoot
    . (Join-Path $root 'scripts/installation-common.ps1')
    . (Join-Path $root 'scripts/dfu-common.ps1')
    . (Join-Path $PSScriptRoot 'auto-upload-common.ps1')
    $context = Get-Uc3Context $SketchbookPath $CliPath $ConfigFile
    $board = Read-Uc3Properties (Join-Path $context.Platform 'boards.local.txt')
    $flags = $board['uc3b0256.build.extra_flags']
    foreach ($flag in @('-DUC3BDUINO_USB_AUTO_UPLOAD=1','-DUC3BDUINO_FACTORY_DFU_GP31=1',
        '-DUC3B_CLOCK_SOURCE=2','-DUC3B_OSC0_HZ=12000000UL','-DUC3B_OSC0_CRYSTAL=1')) {
        if ($flag -notin ($flags -split '\s+')) { throw "Test requires the reference-board auto-upload installation; missing $flag. See docs/AUTO_UPLOAD.md." }
    }
    foreach ($id in @('VENDOR','PRODUCT')) {
        if ($flags -notmatch "(?:^|\s)-DUC3BDUINO_USB_$($id)_ID=([1-9][0-9]*)(?:\s|$)") { throw 'Test installation has no configured USB identity.' }
    }
    $output = New-Uc3BuildDirectory $OutputDirectory
    $buildId = [guid]::NewGuid().ToString('D')
    $sketch = Join-Path $output 'source/AutoFlashTest'
    [void](New-Item -ItemType Directory -Path $sketch)
    $source = [IO.File]::ReadAllText((Join-Path $root 'examples/AutoFlashTest/AutoFlashTest.ino'))
    [IO.File]::WriteAllText((Join-Path $sketch 'AutoFlashTest.ino'),
        "#define UC3BDUINO_AUTOFLASH_BUILD_ID `"$buildId`"`n$source", (New-Object Text.UTF8Encoding($false)))
    [IO.File]::WriteAllText((Join-Path $output 'build-id.txt'), $buildId)
    Invoke-Uc3Compile $context $sketch $output
    Write-Host "[PASS] Build: $buildId"
    $firmware = Join-Path $output 'AutoFlashTest/AutoFlashTest.ino.hex'
    if (-not $ToolPath) {
        $local = Read-Uc3Properties (Join-Path $context.Platform 'platform.local.txt')
        $ToolPath = $local['tools.uc3bdfu.path']
    }
    if ($BuildOnly) {
        Invoke-DfuUpload -Firmware $firmware -ToolPath $ToolPath -LegacyDevice $LegacyDevice -CheckOnly
        Write-Host "BUILD ONLY: no USB operations. For initial manual bootstrap: $firmware"
        return
    }
    $application = Invoke-AutoUpload -Firmware $firmware -Port $Port -UsbVendorId $UsbVendorId -UsbProductId $UsbProductId `
        -ToolPath $ToolPath -LegacyDevice $LegacyDevice -DfuTimeoutSeconds $DfuTimeoutSeconds -ApplicationTimeoutSeconds $ApplicationTimeoutSeconds
    $text = Read-AutoIdentity $application.Port $buildId $IdentityTimeoutSeconds (Join-Path $output 'cdc.log')
    Write-Host $text.Trim()
    Write-Host '[PASS] Application identification: unique build, AT32UC3B0256, 12 MHz, USB CDC'
    Write-Host "PASS: automatic hardware cycle completed. Evidence: $output"
    Write-Host 'LED flashing requires visual observation; no UART or ADC hardware claim is made.'
} catch {
    Write-Error -ErrorAction Continue "[FAIL] Auto-upload test: $($_.Exception.Message)"
    if ($_.Exception.Data.Contains('ExitCode')) { exit [int]$_.Exception.Data['ExitCode'] }
    exit 1
}
