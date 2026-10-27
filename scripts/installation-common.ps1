# SPDX-License-Identifier: MIT
#Requires -Version 5.1
Set-StrictMode -Version Latest

function Invoke-Uc3Process {
    param([string]$Program, [string[]]$Arguments, [hashtable]$Environment = @{},
          [int]$TimeoutSeconds = 60)
    # Windows CRT argument quoting; no shell evaluation, also for paths with spaces.
    $quoted = foreach ($arg in $Arguments) {
        '"' + [regex]::Replace([regex]::Replace($arg, '(\\*)"', '$1$1\"'), '(\\+)$', '$1$1') + '"'
    }
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $Program
    $info.Arguments = $quoted -join ' '
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($key in $Environment.Keys) { $info.EnvironmentVariables[$key] = $Environment[$key] }
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $info
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            $process.Kill()
            $process.WaitForExit()
            throw "Timed out after $TimeoutSeconds seconds: $Program"
        }
        $output = $stdout.GetAwaiter().GetResult()
        $errors = $stderr.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) {
            throw "'$Program' exited with $($process.ExitCode).`n$output`n$errors"
        }
        return [pscustomobject]@{ Output = $output; Errors = $errors }
    } finally { $process.Dispose() }
}

function Resolve-Uc3Toolchain([string]$ToolchainPath) {
    if (-not $ToolchainPath) { $ToolchainPath = $env:AVR32_TOOLCHAIN_PATH }
    if ($ToolchainPath) {
        $bin = [IO.Path]::GetFullPath($ToolchainPath)
        if (Test-Path -LiteralPath (Join-Path $bin 'bin/avr32-gcc.exe') -PathType Leaf) {
            $bin = Join-Path $bin 'bin'
        }
    } else {
        $gcc = Get-Command avr32-gcc.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $gcc) {
            throw 'AVR32 toolchain not found. Set AVR32_TOOLCHAIN_PATH or -ToolchainPath to the legacy AVR32 GNU root or bin directory. See docs/INSTALL_WINDOWS.md; AVR 8-bit and Arm compilers do not work.'
        }
        $bin = Split-Path -Parent $gcc.Source
    }
    # These paths are written to Arduino property files. Braces expand properties.
    if ($bin -match '[\r\n{}"]') { throw 'Unsupported character in toolchain path (newline, brace or quote).' }
    foreach ($tool in @('gcc','g++','as','ld','ar','objcopy','size')) {
        if (-not (Test-Path -LiteralPath (Join-Path $bin "avr32-$tool.exe") -PathType Leaf)) {
            throw "Incomplete AVR32 toolchain: missing avr32-$tool.exe in '$bin'."
        }
    }
    $compiler = Join-Path $bin 'avr32-gcc.exe'
    $version = (Invoke-Uc3Process $compiler @('--version')).Output.Trim()
    $machine = (Invoke-Uc3Process $compiler @('-dumpmachine')).Output.Trim()
    if ($machine -notmatch '^avr32($|-)') { throw "Expected AVR32 compiler, got '$machine'." }
    $multilib = (Invoke-Uc3Process $compiler @('-mpart=uc3b0256','-print-multi-directory')).Output.Trim()
    if ($multilib -ne 'ucr1') { throw "uc3b0256 must select ucr1, got '$multilib'. See docs/TOOLCHAIN.md." }
    $cpp = Join-Path $bin 'avr32-g++.exe'
    if ((Invoke-Uc3Process $cpp @('-dumpmachine')).Output.Trim() -ne $machine -or
        (Invoke-Uc3Process $cpp @('-dumpversion')).Output.Trim() -ne
        (Invoke-Uc3Process $compiler @('-dumpversion')).Output.Trim()) {
        throw 'avr32-gcc and avr32-g++ must have matching targets and versions.'
    }
    return [pscustomobject]@{ Bin = $bin; Version = ($version -split '\r?\n')[0] }
}

function Resolve-Uc3Cli([string]$CliPath) {
    if ($CliPath) {
        if (-not (Test-Path -LiteralPath $CliPath -PathType Leaf)) { throw "Arduino CLI not found: $CliPath" }
        return [IO.Path]::GetFullPath($CliPath)
    }
    $command = Get-Command arduino-cli.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) { return $command.Source }
    foreach ($candidate in @(
        "$env:ProgramFiles/Arduino CLI/arduino-cli.exe",
        "$env:ProgramFiles/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe",
        "$env:LOCALAPPDATA/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
    )) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return [IO.Path]::GetFullPath($candidate) }
    }
    return $null
}

function Get-Uc3Context([string]$SketchbookPath, [string]$CliPath, [string]$ConfigFile) {
    $cli = Resolve-Uc3Cli $CliPath
    if (-not $ConfigFile) {
        $ideConfig = Join-Path $env:USERPROFILE '.arduinoIDE/arduino-cli.yaml'
        if (Test-Path -LiteralPath $ideConfig -PathType Leaf) { $ConfigFile = $ideConfig }
    }
    $cliArgs = @()
    if ($ConfigFile) {
        if (-not (Test-Path -LiteralPath $ConfigFile -PathType Leaf)) { throw "Arduino configuration not found: $ConfigFile" }
        $ConfigFile = [IO.Path]::GetFullPath($ConfigFile)
        $cliArgs = @('--config-file', $ConfigFile)
    }
    $source = 'explicit -SketchbookPath'
    if (-not $SketchbookPath) {
        if ($cli) {
            $SketchbookPath = (Invoke-Uc3Process $cli ($cliArgs + @('config','get','directories.user'))).Output.Trim()
            if (-not $SketchbookPath) { throw 'Arduino returned an empty sketchbook path. Pass -SketchbookPath from IDE Preferences.' }
            $source = 'Arduino CLI resolved directories.user (IDE config preferred)'
        } elseif ($env:ARDUINO_DIRECTORIES_USER) {
            $SketchbookPath = $env:ARDUINO_DIRECTORIES_USER
            $source = 'ARDUINO_DIRECTORIES_USER'
        } elseif ($ConfigFile) {
            throw 'Arduino config found, but CLI is unavailable to read it. Pass -CliPath or -SketchbookPath from IDE Preferences.'
        } else {
            throw 'Cannot detect the sketchbook without Arduino CLI/configuration. Pass -SketchbookPath from IDE Preferences and install Arduino IDE 2.x or CLI.'
        }
    }
    if (-not [IO.Path]::IsPathRooted($SketchbookPath)) { throw 'Sketchbook path must be absolute.' }
    $sketchbook = [IO.Path]::GetFullPath($SketchbookPath)
    return [pscustomobject]@{
        Cli = $cli; CliArgs = $cliArgs; Sketchbook = $sketchbook; Source = $source
        Platform = (Join-Path $sketchbook 'hardware/uc3bduino/avr32')
        Environment = @{ ARDUINO_DIRECTORIES_USER = $sketchbook }
    }
}

function Read-Uc3Properties([string]$Path) {
    $properties = @{}
    foreach ($line in [IO.File]::ReadAllLines($Path)) {
        if ($line -match '^\s*([^#=\s]+)=(.*)$') { $properties[$Matches[1]] = $Matches[2].Trim() }
    }
    return $properties
}

function Test-Uc3DfuTool([string]$ToolPath) {
    . (Join-Path $PSScriptRoot 'dfu-common.ps1')
    $tool = Resolve-DfuTool $ToolPath
    if ($tool -match '[\r\n{}"]') { throw 'Unsupported character in DFU tool path.' }
    $result = Invoke-Uc3Process $tool @('--version')
    $version = ($result.Output + $result.Errors).Trim()
    if ($version -ne 'dfu-programmer 1.1.0') { throw "Expected dfu-programmer 1.1.0, got '$version'. See docs/DFU_UPLOAD.md." }
    return $tool
}

function New-Uc3BuildDirectory([string]$OutputDirectory) {
    if (-not $OutputDirectory) { $OutputDirectory = Join-Path ([IO.Path]::GetTempPath()) ('UC3Bduino-' + [guid]::NewGuid().ToString('N')) }
    $path = [IO.Path]::GetFullPath($OutputDirectory)
    if (Test-Path -LiteralPath $path) { throw "Build output already exists: '$path'. Choose a new directory; no existing files are removed." }
    [void](New-Item -ItemType Directory -Path $path)
    return $path
}

function Invoke-Uc3Compile($Context, [string]$Sketch, [string]$BuildRoot) {
    if (-not $Context.Cli) { throw 'Arduino CLI not found. Install it or pass -CliPath.' }
    $name = Split-Path -Leaf $Sketch
    $build = Join-Path $BuildRoot $name
    $log = Join-Path $BuildRoot ($name + '.log')
    try {
        $result = Invoke-Uc3Process $Context.Cli ($Context.CliArgs + @('compile', '--fqbn',
            'uc3bduino:avr32:uc3b0256', '--build-path', $build, '--verbose', $Sketch)) $Context.Environment 300
        [IO.File]::WriteAllText($log, $result.Output + $result.Errors)
    } catch {
        [IO.File]::WriteAllText($log, $_.Exception.Message)
        throw "Compile failed for $name. See $log`n$($_.Exception.Message)"
    }
    foreach ($extension in @('elf','hex','bin','map')) {
        if (-not (Test-Path -LiteralPath (Join-Path $build "$name.ino.$extension") -PathType Leaf)) {
            throw "Compiler reported success but $name.ino.$extension is missing. See $log"
        }
    }
    Write-Host "PASS: $name compile (log: $log)"
}
