# SPDX-License-Identifier: MIT
#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$ToolchainPath = $env:AVR32_TOOLCHAIN_PATH,
    [ValidateSet('minimal', 'startup-check')]
    [string]$Example = 'minimal'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "'$Program' failed with exit code $LASTEXITCODE."
    }
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$oldPath = $env:PATH
try {
    if ($ToolchainPath) {
        $candidate = [IO.Path]::GetFullPath($ToolchainPath)
        if (Test-Path -LiteralPath (Join-Path $candidate 'bin/avr32-gcc.exe')) {
            $binDirectory = Join-Path $candidate 'bin'
        } elseif (Test-Path -LiteralPath (Join-Path $candidate 'avr32-gcc.exe')) {
            $binDirectory = $candidate
        } else {
            throw "No avr32-gcc.exe in '$ToolchainPath' or its bin directory. Set AVR32_TOOLCHAIN_PATH to an AVR32 GNU toolchain root or bin directory."
        }
    } else {
        $compiler = Get-Command avr32-gcc.exe -CommandType Application -ErrorAction SilentlyContinue
        if (-not $compiler) {
            throw 'AVR32 toolchain not found. Install the legacy Atmel AVR32 GNU toolchain and set AVR32_TOOLCHAIN_PATH to its root or bin directory. AVR 8-bit avr-gcc and Arm GCC are not compatible.'
        }
        $binDirectory = Split-Path -Parent $compiler.Source
    }

    foreach ($tool in @('gcc', 'as', 'ld', 'objcopy', 'size')) {
        if (-not (Test-Path -LiteralPath (Join-Path $binDirectory "avr32-$tool.exe") -PathType Leaf)) {
            throw "Incomplete AVR32 toolchain: missing avr32-$tool.exe in '$binDirectory'."
        }
    }

    # Prefer the selected installation, even when another compiler is on PATH.
    $env:PATH = "$binDirectory;$oldPath"
    $make = Get-Command make.exe, mingw32-make.exe -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $make) {
        throw 'GNU Make 3.81 or newer is required. Add make.exe or mingw32-make.exe to PATH (the historical AVR32 toolchain includes make.exe).'
    }
    $gcc = Join-Path $binDirectory 'avr32-gcc.exe'
    Invoke-Checked $gcc @('--version')
    $machine = Invoke-Checked $gcc @('-dumpmachine')
    if (($machine -join '').Trim() -notmatch '^avr32($|-)') {
        throw "Expected an AVR32 compiler, got '$machine'."
    }
    $multilib = Invoke-Checked $gcc @('-mpart=uc3b0256', '-print-multi-directory')
    if (($multilib -join '').Trim() -ne 'ucr1') {
        throw "Expected uc3b0256 to select ucr1, got '$multilib'. Verify the compiler against docs/TOOLCHAIN.md."
    }

    $buildDirectory = "build/at32uc3b0256/$Example"
    $source = 'src/main.c'
    if ($Example -eq 'startup-check') {
        $source = 'examples/startup_check/main.c'
    }
    New-Item -ItemType Directory -Force -Path (Join-Path $repoRoot $buildDirectory) | Out-Null
    Push-Location -LiteralPath $repoRoot
    try {
        # Stage 1 has two objects: always rebuild to avoid stale flags, map
        # files, or object files after changing the selected toolchain.
        Invoke-Checked $make.Source @('--no-print-directory', '-B', '-f', 'Makefile',
            "TOOLCHAIN_BIN=$($binDirectory.Replace('\', '/'))",
            "BUILD_DIR=$buildDirectory", "APP_SOURCE=$source", 'all')
    } finally {
        Pop-Location
    }
    Write-Host "Build complete: $(Join-Path $repoRoot $buildDirectory)"
} catch {
    Write-Error -ErrorAction Continue "AVR32 build failed: $($_.Exception.Message)"
    exit 1
} finally {
    $env:PATH = $oldPath
}
