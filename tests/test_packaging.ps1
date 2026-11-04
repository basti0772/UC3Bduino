# SPDX-License-Identifier: MIT
#Requires -Version 5.1
# Host-only installation tests. The fake compiler is never a firmware build.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $root 'scripts/installation-common.ps1')
$work = Join-Path $root ('.scratch/packaging tests-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $work)
$script:passed = 0
function Assert([bool]$Condition, [string]$Name) {
    if (-not $Condition) { throw "FAIL: $Name" }
    $script:passed++; Write-Host "PASS: $Name"
}
function Reject([scriptblock]$Action, [string]$Pattern, [string]$Name) {
    $errorText = ''
    try { & $Action | Out-Null } catch { $errorText = $_.Exception.Message }
    # Windows PowerShell formats native stderr to the console width. CI can wrap
    # a diagnostic between words even though the installer rejected correctly.
    $normalized = [regex]::Replace($errorText, '\s+', ' ')
    if ($normalized -notmatch $Pattern) { throw "FAIL: $Name. Expected '$Pattern', received: $errorText" }
    Assert $true $Name
}
function Run-Script([string]$Relative, [string[]]$ScriptArgs) {
    Invoke-Uc3Process 'powershell.exe' (@('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',
        (Join-Path $root $Relative)) + $ScriptArgs) @{} 120
}
$environmentNames = @('PATH','AVR32_TOOLCHAIN_PATH','UC3B_DFU_TOOL_PATH','ARDUINO_DIRECTORIES_USER',
    'UC3_TEST_SKETCHBOOK','UC3_TEST_BAD_TARGET','UC3_TEST_BAD_CPP','UC3_TEST_DFU_VERSION')
$saved = @{}
foreach ($name in $environmentNames) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    $bin = Join-Path $work 'toolchain with spaces/bin'
    [void](New-Item -ItemType Directory -Path $bin)
    $fixture = Join-Path $bin 'avr32-gcc.exe'
    Add-Type -OutputAssembly $fixture -OutputType ConsoleApplication -TypeDefinition @'
using System;
using System.IO;
class Fixture {
    static int Main(string[] args) {
        string name = Path.GetFileNameWithoutExtension(System.Reflection.Assembly.GetExecutingAssembly().Location);
        string joined = String.Join(" ", args);
        if (name == "arduino-cli") {
            if (joined.EndsWith("config get directories.user")) { Console.WriteLine(Environment.GetEnvironmentVariable("UC3_TEST_SKETCHBOOK")); return 0; }
            return 7; // No compile or upload is implemented by this fixture.
        }
        if (name == "dfu-programmer") {
            if (joined != "--version") return 8; // Reject every device/protocol operation.
            Console.WriteLine("dfu-programmer " + (Environment.GetEnvironmentVariable("UC3_TEST_DFU_VERSION") ?? "1.1.0")); return 0;
        }
        if (joined == "--version") Console.WriteLine("avr32 fixture 4.3.2 (host test only)");
        else if (joined == "-dumpmachine") Console.WriteLine(Environment.GetEnvironmentVariable("UC3_TEST_BAD_TARGET") ?? "avr32");
        else if (joined == "-dumpversion") Console.WriteLine(name == "avr32-g++" ? (Environment.GetEnvironmentVariable("UC3_TEST_BAD_CPP") ?? "4.3.2") : "4.3.2");
        else if (joined == "-mpart=uc3b0256 -print-multi-directory") Console.WriteLine("ucr1");
        else return 9;
        return 0;
    }
}
'@
    foreach ($tool in @('g++','as','ld','ar','objcopy','size')) { Copy-Item -LiteralPath $fixture -Destination (Join-Path $bin "avr32-$tool.exe") }
    $cli = Join-Path $bin 'arduino-cli.exe'
    $dfu = Join-Path $bin 'dfu-programmer.exe'
    Copy-Item -LiteralPath $fixture -Destination $cli
    Copy-Item -LiteralPath $fixture -Destination $dfu
    $env:AVR32_TOOLCHAIN_PATH = Split-Path -Parent $bin
    $env:UC3B_DFU_TOOL_PATH = $dfu
    $env:PATH = "$bin;$env:PATH"
    $env:ARDUINO_DIRECTORIES_USER = $null
    $env:UC3_TEST_BAD_TARGET = $null
    $env:UC3_TEST_BAD_CPP = $null
    $env:UC3_TEST_DFU_VERSION = $null
    Assert ((Resolve-Uc3Toolchain '').Bin -eq $bin) 'environment toolchain root resolves to bin with spaces'
    Assert ((Resolve-Uc3Toolchain $bin).Bin -eq $bin) 'explicit bin resolves'
    Reject { Resolve-Uc3Toolchain (Join-Path $work 'missing') } 'missing avr32-gcc' 'invalid explicit toolchain never falls back'
    $env:AVR32_TOOLCHAIN_PATH = $null
    Assert ((Resolve-Uc3Toolchain '').Bin -eq $bin) 'PATH toolchain resolves'
    $env:AVR32_TOOLCHAIN_PATH = Split-Path -Parent $bin
    $env:UC3_TEST_BAD_TARGET = 'arm-none-eabi'
    Reject { Resolve-Uc3Toolchain $bin } 'Expected AVR32' 'reject other compiler target'
    $env:UC3_TEST_BAD_TARGET = $null
    $env:UC3_TEST_BAD_CPP = '4.4.7'
    Reject { Resolve-Uc3Toolchain $bin } 'matching targets and versions' 'reject mismatched C/C++ compiler'
    $env:UC3_TEST_BAD_CPP = $null
    Assert ((Test-Uc3DfuTool $dfu) -eq $dfu) 'DFU version-only validation'
    $env:UC3_TEST_DFU_VERSION = '9.9.9'
    Reject { Test-Uc3DfuTool $dfu } 'Expected dfu-programmer 1.1.0' 'reject unvalidated DFU version'
    $env:UC3_TEST_DFU_VERSION = $null
    $env:UC3_TEST_SKETCHBOOK = Join-Path $work 'configured sketchbook'
    $config = Join-Path $work 'custom cli.yaml'
    [IO.File]::WriteAllText($config, 'directories: {}')
    $context = Get-Uc3Context '' $cli $config
    Assert ($context.Sketchbook -eq $env:UC3_TEST_SKETCHBOOK -and $context.CliArgs[1] -eq $config) 'read configured sketchbook through CLI'
    Assert ((Get-Uc3Context $work $cli $config).Sketchbook -eq $work) 'explicit sketchbook wins'
    Reject { Get-Uc3Context 'relative/path' $cli $config } 'absolute' 'reject ambiguous relative sketchbook'
    $argsInstall = @('-CliPath',$cli,'-ConfigFile',$config,'-GpioPort','A','-GpioPin','3')
    [void](Run-Script 'scripts/install.ps1' ($argsInstall + @('-WhatIf')))
    Assert (-not (Test-Path -LiteralPath $context.Platform)) 'WhatIf creates no platform'
    [void](Run-Script 'scripts/install.ps1' $argsInstall)
    Assert (Test-Path -LiteralPath (Join-Path $context.Platform 'libraries/UC3BduinoExamples/examples/USBSerialBlink/USBSerialBlink.ino')) 'installer assembles bundled examples'
    Assert ((Get-FileHash -LiteralPath (Join-Path $context.Platform 'cores/uc3bduino/SPI.cpp')).Hash -eq
        (Get-FileHash -LiteralPath (Join-Path $root 'arduino/hardware/uc3bduino/avr32/cores/uc3bduino/SPI.cpp')).Hash) 'installer preserves SPI core source'
    Assert (Test-Path -LiteralPath (Join-Path $context.Platform 'libraries/UC3BduinoExamples/examples/SPILoopback/SPILoopback.ino')) 'installer includes SPI loopback example'
    Assert ((Get-FileHash -LiteralPath (Join-Path $context.Platform 'cores/uc3bduino/Wire.cpp')).Hash -eq
        (Get-FileHash -LiteralPath (Join-Path $root 'arduino/hardware/uc3bduino/avr32/cores/uc3bduino/Wire.cpp')).Hash) 'installer preserves Wire core source'
    Assert (Test-Path -LiteralPath (Join-Path $context.Platform 'libraries/UC3BduinoExamples/examples/WireApiSmoke/WireApiSmoke.ino')) 'installer includes Wire API smoke example'
    $props = Read-Uc3Properties (Join-Path $context.Platform 'platform.local.txt')
    Assert ($props['compiler.path'] -eq ($bin.Replace('\','/') + '/') -and $props['tools.uc3bdfu.path'] -eq $dfu.Replace('\','/')) 'installer pins selected compiler and DFU paths'
    $manifest = Get-Content -LiteralPath (Join-Path $context.Platform 'shared-sources.json') -Raw | ConvertFrom-Json
    foreach ($entry in $manifest) {
        if ((Get-FileHash -LiteralPath (Join-Path $context.Platform $entry.installed)).Hash -ne $entry.sha256 -or
            (Get-FileHash -LiteralPath (Join-Path $root $entry.source)).Hash -ne $entry.sha256) { throw "Shared copy mismatch: $($entry.source)" }
    }
    Assert ($manifest.Count -gt 40) 'all shared source/example/document hashes match'
    $sentinel = Join-Path $context.Platform 'user-owned.txt'
    [IO.File]::WriteAllText($sentinel,'keep me')
    Reject { Run-Script 'scripts/install.ps1' $argsInstall } 'already exists' 'existing package is never overwritten'
    Assert ([IO.File]::ReadAllText($sentinel) -eq 'keep me') 'existing user data retained'
    $invalidBook = Join-Path $work 'invalid board'
    Reject { Run-Script 'scripts/install.ps1' @('-SketchbookPath',$invalidBook,'-GpioPort','A','-GpioPin','18','-Osc0Hz','12000000','-Osc0Mode','Crystal') } 'conflicts with an OSC0 pad' 'reject conflicting board configuration'
    Assert (-not (Test-Path -LiteralPath $invalidBook)) 'invalid configuration makes no destination'
    $defaultBook = Join-Path $work 'reference board defaults'
    [void](Run-Script 'scripts/install.ps1' @('-SketchbookPath',$defaultBook))
    $defaultPlatform = Join-Path $defaultBook 'hardware/uc3bduino/avr32'
    $boardProps = Read-Uc3Properties (Join-Path $defaultPlatform 'boards.local.txt')
    Assert ($boardProps['uc3b0256.build.f_cpu'] -eq '12000000L' -and $boardProps['uc3b0256.build.extra_flags'] -match 'UC3B_OSC0_HZ=12000000UL' -and $boardProps['uc3b0256.build.extra_flags'] -match 'UC3B_OSC0_CRYSTAL=1') 'reference board defaults to 12 MHz crystal'
    Assert ($boardProps['uc3b0256.build.extra_flags'] -notmatch 'USB_AUTO_UPLOAD|FACTORY_DFU_GP31') 'auto-upload backend is off by default'
    $autoBook = Join-Path $work 'auto-upload opt-in'
    $autoArgs = @('-SketchbookPath',$autoBook,'-EnableLegacyAutoUpload','-UsbVendorId','65535','-UsbProductId','65534','-LegacyDevice','AT32UC3B0256')
    [void](Run-Script 'scripts/install.ps1' $autoArgs)
    $autoPlatform = Join-Path $autoBook 'hardware/uc3bduino/avr32'
    $autoProps = Read-Uc3Properties (Join-Path $autoPlatform 'boards.local.txt')
    Assert ($autoProps['uc3b0256.build.extra_flags'] -match 'USB_AUTO_UPLOAD=1' -and $autoProps['uc3b0256.build.extra_flags'] -match 'FACTORY_DFU_GP31=1') 'explicit option enables portable touch and legacy backend together'
    Assert (Test-Path -LiteralPath (Join-Path $autoPlatform 'tools/AutoUpload.ps1')) 'installed platform includes auto uploader'
    $autoLocal = Read-Uc3Properties (Join-Path $autoPlatform 'platform.local.txt')
    Assert ($autoLocal['tools.uc3bdfu.upload.pattern'].Contains('tools/AutoUpload.ps1') -and $autoLocal['tools.uc3bdfu.upload.pattern'].Contains('-Port "{serial.port}"') -and $autoProps['uc3b0256.upload.use_1200bps_touch'] -eq 'false') 'opt-in Arduino recipe delegates touch once to AutoUpload'
    Assert (-not $props.ContainsKey('tools.uc3bdfu.upload.pattern')) 'default install retains manual DFU recipe'
    Reject { Run-Script 'scripts/install.ps1' @('-SketchbookPath',$invalidBook,'-EnableLegacyAutoUpload') } 'Legacy auto-upload requires' 'auto-upload rejects missing USB identity and device confirmation'
    Reject { Run-Script 'scripts/install.ps1' @('-SketchbookPath',$invalidBook,'-EnableLegacyAutoUpload','-UsbVendorId','65535','-UsbProductId','65534','-LegacyDevice','AT32UC3B0256','-Osc0Hz','16000000','-Osc0Mode','Crystal') } 'Legacy auto-upload requires' 'auto-upload rejects untested 16 MHz profile'
    Assert ([IO.File]::ReadAllText((Join-Path $defaultPlatform 'variants/uc3b0256/test_pin_config.h')).Contains('#define UC3BDUINO_TEST_PIN_BIT 23')) 'reference D0 defaults to onboard LED PA23'
    $rcBook = Join-Path $work 'explicit internal RC'
    [void](Run-Script 'scripts/install.ps1' @('-SketchbookPath',$rcBook,'-InternalRc'))
    $rcProps = Read-Uc3Properties (Join-Path $rcBook 'hardware/uc3bduino/avr32/boards.local.txt')
    Assert ($rcProps['uc3b0256.build.f_cpu'] -eq '115200L' -and $rcProps['uc3b0256.build.extra_flags'] -match 'UC3B_CLOCK_SOURCE=1' -and $rcProps['uc3b0256.build.extra_flags'] -notmatch 'UC3B_OSC0_HZ') 'explicit RC overrides board clock and F_CPU coherently'
    Reject { Run-Script 'scripts/install.ps1' @('-SketchbookPath',$invalidBook,'-InternalRc','-UsbVendorId','1','-UsbProductId','1') } 'USB CDC requires' 'reject USB identity with internal RC'
    Reject { Run-Script 'scripts/install.ps1' @('-SketchbookPath',$invalidBook,'-SerialRoute','U1A','-GpioPort','B','-GpioPin','0') } 'onboard LED pad PA23' 'reject UART conflict with fixed LED even when D0 moves'
    $badZip = Join-Path $work 'invalid.zip'
    [IO.File]::WriteAllText($badZip,'not an upstream tool')
    $badDestination = Join-Path $work 'bad download'
    Reject { Run-Script 'scripts/get-dfu-tool.ps1' @('-ArchivePath',$badZip,'-Destination',$badDestination) } 'size/SHA-256 mismatch' 'DFU download rejects untrusted archive'
    Assert (-not (Test-Path -LiteralPath $badDestination)) 'invalid DFU archive never extracted'
    $out1 = Join-Path $work 'package one'
    $out2 = Join-Path $work 'package two'
    [void](Run-Script 'scripts/package-release.ps1' @('-OutputDirectory',$out1))
    [void](Run-Script 'scripts/package-release.ps1' @('-OutputDirectory',$out2))
    $zip1 = Get-ChildItem -LiteralPath $out1 -Filter *.zip
    $zip2 = Get-ChildItem -LiteralPath $out2 -Filter *.zip
    Assert ((Get-FileHash -LiteralPath $zip1.FullName).Hash -eq (Get-FileHash -LiteralPath $zip2.FullName).Hash) 'unchanged sources yield identical ZIPs'
    Reject { Run-Script 'scripts/package-release.ps1' @('-OutputDirectory',$out1) } 'already exists' 'archive output cannot overwrite files'
    $expanded = Join-Path $work 'expanded source'
    Expand-Archive -LiteralPath $zip1.FullName -DestinationPath $expanded
    $sourceRoot = (Get-ChildItem -LiteralPath $expanded -Directory).FullName
    $files = Get-ChildItem -LiteralPath $sourceRoot -Recurse -File
    Assert (@($files | Where-Object { $_.Extension -in @('.exe','.dll','.elf','.bin','.hex','.zip') -or $_.Name -like '*.local.txt' }).Count -eq 0) 'archive contains no native binaries or local tool paths'
    foreach ($line in Get-Content -LiteralPath (Join-Path $sourceRoot 'SOURCE_SHA256SUMS.txt')) {
        $parts = $line -split '  ',2
        if ((Get-FileHash -LiteralPath (Join-Path $sourceRoot $parts[1])).Hash.ToLowerInvariant() -ne $parts[0]) { throw "Archive hash mismatch: $($parts[1])" }
    }
    Assert ($files.Count -gt 100) 'all extracted source checksums match'
    $freshBook = Join-Path $work 'from archive'
    [void](Invoke-Uc3Process 'powershell.exe' @('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',
        (Join-Path $sourceRoot 'scripts/install.ps1'),'-SketchbookPath',$freshBook) @{} 120)
    Assert (Test-Path -LiteralPath (Join-Path $freshBook 'hardware/uc3bduino/avr32/cores/uc3bduino/startup/uc3_startup.S')) 'extracted archive installs without Git'
    Write-Host "PASS: $script:passed packaging host checks. Fixtures and archives retained at $work"
} finally {
    foreach ($name in $environmentNames) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
}
