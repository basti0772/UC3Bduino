# SPDX-License-Identifier: MIT
param([Parameter(Mandatory=$true)][string]$Compiler)
$ErrorActionPreference = 'Stop'
if (-not (Get-Command $Compiler -CommandType Application -ErrorAction SilentlyContinue)) {
    throw "AVR32 GCC is missing. Install the Windows AVR32 toolchain and run tools/setup-boards-manager.ps1 in the installed platform. See docs/INSTALL_ARDUINO_WINDOWS.md."
}
