# SPDX-License-Identifier: MIT
#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Firmware,
    [string]$Port,
    [ValidateRange(0,65535)][int]$UsbVendorId,
    [ValidateRange(0,65535)][int]$UsbProductId,
    [string]$ToolPath,
    [ValidateSet('Unconfirmed','AT32UC3B0256')][string]$LegacyDevice = 'Unconfirmed',
    [ValidateRange(1,120)][int]$DfuTimeoutSeconds = 20,
    [ValidateRange(1,120)][int]$ApplicationTimeoutSeconds = 30
)
$ErrorActionPreference = 'Stop'
try {
    # Works from a checkout and from the installer's platform/tools directory.
    $dfuCommon = Join-Path $PSScriptRoot 'dfu-common.ps1'
    if (-not (Test-Path -LiteralPath $dfuCommon)) { $dfuCommon = Join-Path (Split-Path -Parent $PSScriptRoot) 'scripts/dfu-common.ps1' }
    . $dfuCommon
    . (Join-Path $PSScriptRoot 'auto-upload-common.ps1')
    Invoke-AutoUpload @PSBoundParameters
} catch {
    Write-Error -ErrorAction Continue "[FAIL] Automatic upload: $($_.Exception.Message)"
    if ($_.Exception.Data.Contains('ExitCode')) { exit [int]$_.Exception.Data['ExitCode'] }
    exit 1
}
