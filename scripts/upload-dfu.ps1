# SPDX-License-Identifier: MIT
#Requires -Version 5.1
[CmdletBinding(DefaultParameterSetName = 'Upload')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Upload')]
    [string]$Firmware,
    [string]$ToolPath,
    [ValidateSet('Unconfirmed','AT32UC3B0256')]
    [string]$LegacyDevice = 'Unconfirmed',
    [Parameter(ParameterSetName = 'Upload')]
    [switch]$CheckOnly,
    [Parameter(Mandatory = $true, ParameterSetName = 'Probe')]
    [switch]$ProbeOnly
)
$ErrorActionPreference = 'Stop'
try {
    . (Join-Path $PSScriptRoot 'dfu-common.ps1')
    Invoke-DfuUpload -Firmware $Firmware -ToolPath $ToolPath -LegacyDevice $LegacyDevice -CheckOnly:$CheckOnly -ProbeOnly:$ProbeOnly
} catch {
    Write-Error -ErrorAction Continue "UC3Bduino upload failed: $($_.Exception.Message)"
    exit 1
}
