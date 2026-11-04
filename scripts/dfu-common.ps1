# SPDX-License-Identifier: MIT
#Requires -Version 5.1
# Shared by the manual uploader, installed Arduino recipe, and host tests.
Set-StrictMode -Version Latest

function Resolve-DfuTool([string]$ToolPath) {
    if (-not $ToolPath -or $ToolPath -eq 'auto') { $ToolPath = $env:UC3B_DFU_TOOL_PATH }
    if ($ToolPath) {
        if (Test-Path -LiteralPath $ToolPath -PathType Container) {
            $ToolPath = Join-Path $ToolPath 'dfu-programmer.exe'
        }
        if (-not (Test-Path -LiteralPath $ToolPath -PathType Leaf)) {
            throw "Upload tool not installed: '$ToolPath'. Set UC3B_DFU_TOOL_PATH to dfu-programmer 1.1.0 (directory or .exe)."
        }
        $tool = (Get-Item -LiteralPath $ToolPath).FullName
    } else {
        $command = Get-Command dfu-programmer.exe -CommandType Application -ErrorAction SilentlyContinue
        if (-not $command) { throw 'Upload tool not installed. Set UC3B_DFU_TOOL_PATH to the extracted dfu-programmer 1.1.0 directory.' }
        $tool = $command.Source
    }
    if ([IO.Path]::GetExtension($tool) -ne '.exe') { throw 'Upload tool must be a Windows .exe.' }
    return $tool
}

function Test-DfuHex([string]$Text) {
    # Use Int64 decimal addresses: PowerShell 5.1 parses 0x80000000 as signed.
    $first = 2147491840L # 0x80002000
    $limit = 2147745792L # 0x80040000 (exclusive)
    $seen = New-Object 'System.Collections.Generic.HashSet[long]'
    $upper = 0L
    $eof = $false
    $startSeen = $false
    $entryNotBlank = $false
    $reader = New-Object IO.StringReader($Text)
    $lineNumber = 0
    try {
        while ($null -ne ($line = $reader.ReadLine())) {
            $lineNumber++
            if ($eof) { throw "HEX record after EOF at line $lineNumber." }
            if ($line -cnotmatch '^:[0-9A-Fa-f]+$' -or ($line.Length % 2) -ne 1 -or $line.Length -lt 11) {
                throw "Malformed Intel HEX at line $lineNumber. Use the generated .hex, not ELF/BIN."
            }
            $count = [Convert]::ToInt32($line.Substring(1,2),16)
            if ($line.Length -ne 11 + 2 * $count) { throw "HEX length mismatch at line $lineNumber." }
            $bytes = New-Object byte[] (($line.Length - 1) / 2)
            $sum = 0
            for ($i = 0; $i -lt $bytes.Length; $i++) {
                $bytes[$i] = [Convert]::ToByte($line.Substring(1 + 2 * $i,2),16)
                $sum += $bytes[$i]
            }
            if (($sum -band 255) -ne 0) { throw "HEX checksum mismatch at line $lineNumber." }
            $offset = 256 * [int]$bytes[1] + [int]$bytes[2]
            $type = $bytes[3]
            # dfu-programmer 1.1.0 treats type 05 as an address offset. Only
            # accept it immediately before EOF, as emitted by avr32-objcopy.
            if ($startSeen -and $type -ne 1) { throw 'Only EOF may follow the HEX start-address record.' }
            switch ($type) {
                0 {
                    if ($count -eq 0 -or $offset + $count -gt 65536) { throw 'Invalid HEX data record extent.' }
                    $address = $upper + $offset
                    if ($address -lt $first -or $address + $count -gt $limit) {
                        throw ('Unsafe firmware address 0x{0:X8}. Only 0x80002000..0x8003FFFF is allowed; DO NOT overwrite the factory USB DFU bootloader.' -f $address)
                    }
                    for ($i = 0; $i -lt $count; $i++) {
                        if (-not $seen.Add($address + $i)) { throw 'Overlapping HEX data records.' }
                        if ($address + $i -lt $first + 4 -and $bytes[4 + $i] -ne 255) { $entryNotBlank = $true }
                    }
                }
                1 {
                    if ($count -ne 0 -or $offset -ne 0) { throw 'Invalid HEX EOF.' }
                    $eof = $true
                }
                4 {
                    if ($count -ne 2 -or $offset -ne 0) { throw 'Invalid HEX extended linear address.' }
                    $upper = (256L * $bytes[4] + $bytes[5]) * 65536L
                }
                5 {
                    if ($count -ne 4 -or $offset -ne 0) { throw 'Invalid HEX start address.' }
                    $start = 16777216L * $bytes[4] + 65536L * $bytes[5] + 256L * $bytes[6] + $bytes[7]
                    if ($start -ne $first) { throw 'HEX application entry must be 0x80002000.' }
                    $startSeen = $true
                }
                default { throw "Unsupported HEX record type $type. Rebuild with the UC3Bduino objcopy recipe." }
            }
        }
    } finally { $reader.Dispose() }
    if (-not $eof -or $seen.Count -eq 0) { throw 'Incomplete or empty Intel HEX firmware.' }
    for ($i = 0; $i -lt 4; $i++) {
        if (-not $seen.Contains($first + $i)) { throw 'Firmware has no complete application entry at 0x80002000.' }
    }
    if (-not $entryNotBlank) { throw 'Application entry is erased (0xFFFFFFFF).' }
    return $seen.Count
}

function Invoke-DfuCommand([string]$Tool, [string[]]$Arguments) {
    # No shell, cmd.exe, or PowerShell expression evaluation. Quote Windows
    # argv including trailing backslashes and literal quotes (CRT convention).
    $quoted = foreach ($arg in $Arguments) {
        '"' + [regex]::Replace([regex]::Replace($arg, '(\\*)"', '$1$1\"'), '(\\+)$', '$1$1') + '"'
    }
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $Tool
    $info.Arguments = $quoted -join ' '
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $info
    try {
        if (-not $process.Start()) { throw 'Could not start upload tool.' }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(60000)) {
            $process.Kill()
            $process.WaitForExit()
            throw 'Upload tool timed out. No later upload step will run; re-enter DFU mode before retrying.'
        }
        $output = $stdout.Result + $stderr.Result
        if ($process.ExitCode -ne 0) {
            $failure = New-Object Exception("dfu-programmer $($Arguments -join ' ') failed (exit $($process.ExitCode)): $($output.Trim())")
            $failure.Data['ExitCode'] = $process.ExitCode
            throw $failure
        }
        return $output
    } finally { $process.Dispose() }
}

function Get-DfuDeviceId {
    # Count physical USB device nodes even if their driver cannot be opened by
    # libusb. dfu-programmer itself chooses the first match, so is insufficient.
    $devices = @(Get-PnpDevice -PresentOnly -ErrorAction Stop | Where-Object {
        $_.InstanceId -match '^USB\\VID_03EB&PID_2FF6(?:&REV_[0-9A-F]{4})?\\'
    })
    if ($devices.Count -eq 0) { throw 'DFU device not found (USB VID 03EB / PID 2FF6). Enter factory DFU mode and check cable/driver.' }
    if ($devices.Count -ne 1) { throw 'Multiple UC3B DFU devices found. Connect exactly one target before uploading.' }
    return $devices[0].InstanceId
}

function Get-DfuValue([string]$Output) {
    $matchesFound = [regex]::Matches($Output, '(?m)^(?:[^\r\n]*: )?0x([0-9a-fA-F]{2}) \(([0-9]+)\)\r?$')
    if ($matchesFound.Count -ne 1) { throw "Unrecognized DFU identity/fuse response: $Output" }
    $value = [Convert]::ToInt32($matchesFound[0].Groups[1].Value,16)
    if ($value -ne [int]$matchesFound[0].Groups[2].Value) { throw 'Inconsistent DFU identity/fuse response.' }
    return $value
}

function Get-DfuUsbDevices([string]$Tool) {
    $library = Join-Path (Split-Path -Parent $Tool) 'libusb-1.0.dll'
    if (-not (Test-Path -LiteralPath $library -PathType Leaf)) { throw "Missing programmer dependency: $library" }
    if (-not ('Uc3DfuUsbLocator' -as [type])) { Add-Type -Path (Join-Path $PSScriptRoot 'dfu-usb.cs') }
    [Uc3DfuUsbLocator]::Find([IO.Path]::GetFullPath($library))
}

function Get-DfuPinnedTarget([string]$Tool) {
    # Windows release builds omit the upstream DEBUG bus/address line. Ask the
    # programmer's own libusb DLL instead; enumeration does not open a device.
    $locations = @(Get-DfuUsbDevices $Tool)
    if ($locations.Count -ne 1 -or $locations[0].Bus -lt 1 -or $locations[0].Bus -gt 255 -or
        $locations[0].Address -lt 1 -or $locations[0].Address -gt 127) {
        throw 'Could not uniquely resolve the DFU USB bus/address. Check WinUSB driver and dfu-programmer 1.1.0.'
    }
    return 'at32uc3b0256:{0},{1}' -f $locations[0].Bus,$locations[0].Address
}

function Get-DfuTarget([string]$Tool, [string]$LegacyDevice) {
    $deviceId = Get-DfuDeviceId
    $target = Get-DfuPinnedTarget $Tool
    $probe = Invoke-DfuCommand $Tool @($target,'get','manufacturer')
    $manufacturer = Get-DfuValue $probe
    $family = Get-DfuValue (Invoke-DfuCommand $Tool @($target,'get','family'))
    $product = Get-DfuValue (Invoke-DfuCommand $Tool @($target,'get','product-name'))
    $revision = Get-DfuValue (Invoke-DfuCommand $Tool @($target,'get','product-revision'))
    $boot = Get-DfuValue (Invoke-DfuCommand $Tool @($target,'get','bootloader-version'))
    if ($manufacturer -eq 0x58 -and $family -eq 0x20 -and $product -eq 0x02 -and $revision -le 15 -and $boot -in @(0,1,2,3)) {
        if ($LegacyDevice -ne 'AT32UC3B0256') {
            throw 'Legacy bootloader identifies only the UC3B family. Verify the actual MCU marking is AT32UC3B0256, then set -LegacyDevice AT32UC3B0256 (installer or uploader). No flash was erased.'
        }
        Write-Host 'Legacy UC3B signature matched; AT32UC3B0256 is explicitly confirmed by local configuration.'
    } elseif (-not ($manufacturer -eq 0x1F -and $family -eq 0x1E -and $product -eq 0xE4 -and $revision -le 15 -and $boot -eq 0x10)) {
        throw ('Unsupported DFU identity {0:X2}/{1:X2}/{2:X2}/{3:X2}, bootloader 0x{4:X2}. Expected AT32UC3B0256; no erase performed.' -f $manufacturer,$family,$product,$revision,$boot)
    }
    $protection = Get-DfuValue (Invoke-DfuCommand $Tool @($target,'getfuse','BOOTPROT'))
    if ($protection -ne 3) { throw "Unsafe/incompatible BOOTPROT=$protection. Required: 3 (8 KiB). Fuses will not be changed." }
    if ((Get-DfuDeviceId) -ne $deviceId -or (Get-DfuPinnedTarget $Tool) -ne $target) { throw 'DFU device changed during detection. Aborting.' }
    Write-Host ('DFU target {0}; signature {1:X2}/{2:X2}/{3:X2}/{4:X2}; bootloader 0x{5:X2}; BOOTPROT=3.' -f $target,$manufacturer,$family,$product,$revision,$boot)
    return @{ Target = $target; DeviceId = $deviceId }
}

function Invoke-DfuUpload([string]$Firmware, [string]$ToolPath, [string]$LegacyDevice, [switch]$CheckOnly, [switch]$ProbeOnly, [hashtable]$ExpectedTarget) {
    $tool = Resolve-DfuTool $ToolPath
    $version = Invoke-DfuCommand $tool @('--version')
    if ($version.Trim() -cne 'dfu-programmer 1.1.0') { throw "Unsupported upload tool version: $($version.Trim()). Install dfu-programmer 1.1.0." }
    $stream = $null
    $mutex = New-Object Threading.Mutex($false, 'Local\UC3BduinoDfuUpload')
    $owned = $false
    try {
        try { $owned = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $owned = $true }
        if (-not $owned) { throw 'Another UC3Bduino upload is running.' }
        if (-not $ProbeOnly) {
            if (-not $Firmware -or -not (Test-Path -LiteralPath $Firmware -PathType Leaf)) { throw "Firmware file not found: '$Firmware'. Build Blink first." }
            $file = Get-Item -LiteralPath $Firmware
            if ($file.Extension -ne '.hex' -or $file.Length -gt 1048576) { throw 'Expected a generated Intel HEX file of at most 1 MiB.' }
            $Firmware = $file.FullName
            # Keep the validated file open without write/delete sharing until
            # flash/readback finish, so a concurrent build cannot replace it.
            $stream = [IO.File]::Open($Firmware,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
            $reader = New-Object IO.StreamReader($stream,[Text.Encoding]::ASCII,$false,4096,$true)
            try { $count = Test-DfuHex $reader.ReadToEnd() } finally { $reader.Dispose() }
            Write-Host "Firmware validated: $count bytes, application region 0x80002000..0x8003FFFF."
        }
        if ($CheckOnly) { Write-Host 'CHECK ONLY: no USB commands were sent.'; return }
        $device = Get-DfuTarget $tool $LegacyDevice
        if ($ExpectedTarget -and ($device.DeviceId -ne $ExpectedTarget.DeviceId -or $device.Target -ne $ExpectedTarget.Target)) {
            throw 'DFU target changed after automatic reboot detection. No erase performed.'
        }
        if ($ProbeOnly) { Write-Host 'PROBE ONLY: no erase, flash, or launch requested. Bootloader commands may set ISP_FORCE; see DFU_UPLOAD.md.'; return }
        foreach ($step in @('erase','flash','launch')) {
            if ((Get-DfuDeviceId) -ne $device.DeviceId -or (Get-DfuPinnedTarget $tool) -ne $device.Target) { throw 'DFU device changed/disconnected. No later upload step will run.' }
            $commandArgs = @($device.Target,$step)
            if ($step -eq 'flash') { $commandArgs += $Firmware }
            Write-Host "DFU $step$(if ($step -eq 'flash') { ' with mandatory readback verification' })..."
            try { Write-Host (Invoke-DfuCommand $tool $commandArgs).Trim() }
            catch {
                $failure = New-Object Exception("DFU $step failed. No later upload step will run. $($_.Exception.Message)", $_.Exception)
                if ($_.Exception.Data.Contains('ExitCode')) { $failure.Data['ExitCode'] = $_.Exception.Data['ExitCode'] }
                throw $failure
            }
        }
        Write-Host 'Upload and readback verification succeeded; application reset/start requested.'
    } finally {
        if ($stream) { $stream.Dispose() }
        if ($owned) { $mutex.ReleaseMutex() }
        $mutex.Dispose()
    }
}
