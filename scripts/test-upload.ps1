# SPDX-License-Identifier: MIT
#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildPath,
    [string]$ToolPath,
    [ValidateSet('Unconfirmed','AT32UC3B0256')]
    [string]$LegacyDevice = 'Unconfirmed',
    [switch]$CheckOnly
)
# Deliberately use the production uploader. Without -CheckOnly this programs
# the connected target with an already built Blink (no separate test uploader).
& (Join-Path $PSScriptRoot 'upload-dfu.ps1') -Firmware (Join-Path $BuildPath 'Blink.ino.hex') -ToolPath $ToolPath -LegacyDevice $LegacyDevice -CheckOnly:$CheckOnly
