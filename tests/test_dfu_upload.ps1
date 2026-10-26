# SPDX-License-Identifier: MIT
#Requires -Version 5.1
# Host tests only. No USB device is opened and no real programmer is executed.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../scripts/dfu-common.ps1')
$script:passed = 0
function Assert-True($Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Expect-Failure([scriptblock]$Action, [string]$Pattern) {
    $caught = $null
    try { & $Action | Out-Null } catch { $caught = $_.Exception.Message }
    Assert-True ($null -ne $caught -and $caught -like "*$Pattern*") "Expected '$Pattern', got '$caught'"
    $script:passed++
}
function Record([int]$Type, [int]$Address, [byte[]]$Data) {
    $bytes = @($Data.Length,($Address -shr 8),($Address -band 255),$Type) + @($Data)
    $sum = 0
    foreach ($b in $bytes) { $sum += $b }
    ':' + (($bytes + @((- $sum) -band 255) | ForEach-Object { '{0:X2}' -f $_ }) -join '')
}
$upper = Record 4 0 @(0x80,0)
$data = Record 0 0x2000 @(0xD7,0x03,0,0)
$end = Record 1 0 @()
$start = Record 5 0 @(0x80,0,0x20,0)
$valid = "$upper`n$data`n$start`n$end`n"
Assert-True ((Test-DfuHex $valid) -eq 4) 'Valid HEX rejected'
$script:passed++
Expect-Failure { Test-DfuHex ($valid.Replace($data,($data.Substring(0,$data.Length-2)+'00'))) } 'checksum'
Expect-Failure { Test-DfuHex "$upper`n$(Record 0 0 @(1,2,3,4))`n$end" } 'Unsafe firmware address'
Expect-Failure { Test-DfuHex "$(Record 4 0 @(0x80,0x80))`n$data`n$end" } 'Unsafe firmware address'
Expect-Failure { Test-DfuHex "$data`n$end" } 'Unsafe firmware address'
Expect-Failure { Test-DfuHex "$(Record 4 0 @(0x80,4))`n$(Record 0 0 @(1))`n$end" } 'Unsafe firmware address'
Expect-Failure { Test-DfuHex "$upper`n$data`n$data`n$end" } 'Overlapping'
Expect-Failure { Test-DfuHex "$upper`n$data" } 'Incomplete'
Expect-Failure { Test-DfuHex "$valid$data" } 'after EOF'
Expect-Failure { Test-DfuHex "$upper`n$data`n$start`n$data`n$end" } 'Only EOF'
Expect-Failure { Test-DfuHex "$upper`n$data`n$(Record 5 0 @(0x80,0,0,0))`n$end" } 'entry must'
Expect-Failure { Test-DfuHex "$upper`n$(Record 0 0x2004 @(1,2,3,4))`n$end" } 'complete application entry'
Expect-Failure { Test-DfuHex "$upper`n$(Record 0 0x2000 @(255,255,255,255))`n$end" } 'entry is erased'
Expect-Failure { Test-DfuHex "$upper`n$(Record 0 65535 @(1,2))`n$end" } 'extent'
Expect-Failure { Test-DfuHex "$(Record 2 0 @(0,0))`n$end" } 'Unsupported HEX record'
Expect-Failure { Test-DfuHex ':04000000AA' } 'length mismatch'
Expect-Failure { Test-DfuHex 'ELF binary' } 'Malformed'
Expect-Failure { Test-DfuHex "$upper`n$data`n$(Record 1 1 @())" } 'Invalid HEX EOF'
Expect-Failure { Test-DfuHex "$upper`n$data`n$(Record 4 1 @(0x80,0))`n$end" } 'extended linear'
Expect-Failure { Get-DfuValue '0x03 (7)' } 'Inconsistent'
Expect-Failure { Get-DfuValue 'unknown' } 'Unrecognized'

# Test Windows enumeration separately, including an inaccessible second device.
$script:pnp = @()
$script:swapAfterErase = $false
function Get-PnpDevice {
    param([switch]$PresentOnly, $ErrorAction)
    if ($script:swapAfterErase -and (Get-Content -Raw $env:UC3B_TEST_DFU_LOG) -match '\|erase') {
        [pscustomobject]@{InstanceId='USB\VID_03EB&PID_2FF6\CHANGED'}
    } else { $script:pnp }
}
Expect-Failure { Get-DfuDeviceId } 'DFU device not found'
$script:pnp = @([pscustomobject]@{InstanceId='USB\VID_03EB&PID_2FF6\A'})
Assert-True ((Get-DfuDeviceId) -eq $script:pnp[0].InstanceId) 'Single device not selected'
$script:passed++
$script:pnp += [pscustomobject]@{InstanceId='USB\VID_03EB&PID_2FF6\B';Status='Error'}
Expect-Failure { Get-DfuDeviceId } 'Multiple UC3B'
$script:pnp = @([pscustomobject]@{InstanceId='USB\VID_03EB&PID_2FF6\A'})

# Native executable fixture exercises actual argument quoting, stderr, exit
# handling and file sharing. Its protocol output is synthetic, not hardware.
$fixtureDir = Join-Path ([IO.Path]::GetTempPath()) ('uc3b-dfu-tests-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $fixtureDir | Out-Null
$fixture = Join-Path $fixtureDir 'fake programmer.exe'
$firmware = Join-Path $fixtureDir 'firmware with spaces & apostrophe''s.hex'
$log = Join-Path $fixtureDir 'commands.txt'
$oldLog = $env:UC3B_TEST_DFU_LOG
$oldScenario = $env:UC3B_TEST_DFU_SCENARIO
try {
    Add-Type -OutputAssembly $fixture -OutputType ConsoleApplication -TypeDefinition @'
using System;
using System.IO;
public class DfuFixture {
    public static int Main(string[] a) {
        string mode = Environment.GetEnvironmentVariable("UC3B_TEST_DFU_SCENARIO") ?? "";
        File.AppendAllText(Environment.GetEnvironmentVariable("UC3B_TEST_DFU_LOG"), String.Join("|",a)+"\n");
        if (a[0] == "--version") { Console.WriteLine("dfu-programmer " + (mode == "version" ? "0.7.2" : "1.1.0")); return 0; }
        if (a[0] == "echo") { Console.WriteLine(a[1]); Console.Error.WriteLine(a[2]); return 0; }
        if (a.Length < 2) return 90;
        if (a[1] == mode) { Console.Error.WriteLine("synthetic failure"); return 7; }
        if (a[1] == "get" || a[1] == "getfuse") {
            int v = 0;
            bool legacy = mode == "legacy";
            switch (a[2]) {
                case "manufacturer": v = legacy ? 0x58 : 0x1f; Console.Error.WriteLine("dfu.c: found device at USB:1,9"); break;
                case "family": v = legacy ? 0x20 : 0x1e; break;
                case "product-name": v = legacy ? 2 : (mode == "wrong-chip" ? 0xe5 : 0xe4); break;
                case "product-revision": v = 5; break;
                case "bootloader-version": v = legacy ? 3 : (mode == "unknown-boot" ? 0x99 : 0x10); break;
                case "BOOTPROT": v = mode == "unprotected" ? 7 : 3; break;
            }
            Console.WriteLine("0x{0:x2} ({0})",v); return 0;
        }
        if (a[1] == "flash") {
            if (a.Length != 3 || !File.Exists(a[2])) return 91;
            try { using (File.Open(a[2],FileMode.Open,FileAccess.Write)) {} return 92; }
            catch (IOException) {} // production uploader must hold a read-only lock
            if (File.ReadAllText(a[2]).Length == 0) return 93;
        }
        Console.Error.WriteLine("synthetic " + a[1] + " success"); return 0;
    }
}
'@
    $env:UC3B_TEST_DFU_LOG = $log
    $env:UC3B_TEST_DFU_SCENARIO = ''
    [IO.File]::WriteAllText($firmware,$valid,[Text.Encoding]::ASCII)
    $echo = Invoke-DfuCommand $fixture @('echo','space & "quote" and trailing\','stderr output')
    Assert-True ($echo -eq "space & `"quote`" and trailing\`r`nstderr output`r`n") 'Native argv quoting/stdout/stderr failed'
    $script:passed++
    function Reset-Log([string]$Scenario) {
        $env:UC3B_TEST_DFU_SCENARIO = $Scenario
        [IO.File]::WriteAllText($log,'')
    }
    function Upload-Fixture([string]$Legacy = 'Unconfirmed') {
        Invoke-DfuUpload -Firmware $firmware -ToolPath $fixture -LegacyDevice $Legacy
    }
    Reset-Log ''
    Upload-Fixture
    $lines = @(Get-Content -LiteralPath $log)
    Assert-True (($lines[-3..-1] -join "`n") -eq "at32uc3b0256:1,9|erase`nat32uc3b0256:1,9|flash|$firmware`nat32uc3b0256:1,9|launch") 'Wrong write sequence/address/validation flags'
    $script:passed++
    foreach ($scenario in @('wrong-chip','unprotected','unknown-boot','version','legacy')) {
        Reset-Log $scenario
        Expect-Failure { Upload-Fixture } $(switch ($scenario) { 'wrong-chip' {'Unsupported DFU identity'} 'unprotected' {'BOOTPROT'} 'unknown-boot' {'Unsupported DFU identity'} 'version' {'Unsupported upload tool version'} 'legacy' {'Legacy bootloader identifies only'} })
        Assert-True (-not ((Get-Content -Raw $log) -match '\|(erase|flash|launch)')) "Unsafe write in $scenario"
    }
    Reset-Log 'legacy'
    Upload-Fixture 'AT32UC3B0256'
    Assert-True ((Get-Content -Raw $log) -match '\|launch') 'Explicit legacy confirmation failed'
    $script:passed++
    foreach ($scenario in @('get','erase','flash','launch')) {
        Reset-Log $scenario
        Expect-Failure { Upload-Fixture } 'failed'
        $lines = @(Get-Content -LiteralPath $log)
        Assert-True ($lines[-1] -match "\|$scenario(?:\||$)") "Continued after $scenario failure"
    }
    Reset-Log ''
    $script:swapAfterErase = $true
    Expect-Failure { Upload-Fixture } 'device changed'
    Assert-True ((Get-Content -LiteralPath $log)[-1] -match '\|erase$') 'Programmed after device changed'
    $script:swapAfterErase = $false
    Reset-Log ''
    Invoke-DfuUpload -Firmware $firmware -ToolPath $fixture -LegacyDevice Unconfirmed -CheckOnly
    Assert-True ((Get-Content -Raw $log).Trim() -eq '--version') 'CheckOnly contacted USB'
    $script:passed++
    Reset-Log ''
    Invoke-DfuUpload -ToolPath $fixture -LegacyDevice Unconfirmed -ProbeOnly
    Assert-True (-not ((Get-Content -Raw $log) -match '\|(erase|flash|launch)')) 'Probe wrote flash'
    $script:passed++
    Reset-Log ''
    [IO.File]::WriteAllText($firmware,"$data`n$end",[Text.Encoding]::ASCII)
    Expect-Failure { Upload-Fixture } 'Unsafe firmware address'
    Assert-True ((Get-Content -Raw $log).Trim() -eq '--version') 'Invalid image reached target'
    Expect-Failure { Invoke-DfuUpload -Firmware (Join-Path $fixtureDir 'missing.hex') -ToolPath $fixture } 'Firmware file not found'
    Expect-Failure { Resolve-DfuTool (Join-Path $fixtureDir 'missing.exe') } 'Upload tool not installed'
    Write-Output "PASS: $script:passed DFU host checks (synthetic device; no hardware upload)."
} finally {
    $env:UC3B_TEST_DFU_LOG = $oldLog
    $env:UC3B_TEST_DFU_SCENARIO = $oldScenario
    # Delete only the unique directory created above, after validating its root.
    $resolved = [IO.Path]::GetFullPath($fixtureDir)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if ($resolved.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($resolved) -like 'uc3b-dfu-tests-*') {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
