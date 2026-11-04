# SPDX-License-Identifier: MIT
#Requires -Version 5.1
Set-StrictMode -Version Latest

function Get-AutoUsbSnapshot {
    foreach ($device in Get-PnpDevice -PresentOnly -ErrorAction Stop) {
        if ($device.InstanceId -notmatch '^USB\\VID_([0-9A-F]{4})&PID_([0-9A-F]{4})(?:&[^\\]+)?\\') { continue }
        $vid = [Convert]::ToInt32($Matches[1],16)
        $pidValue = [Convert]::ToInt32($Matches[2],16)
        $dfu = $device.InstanceId -match '^USB\\VID_03EB&PID_2FF6(?:&REV_[0-9A-F]{4})?\\'
        $portName = ''
        if ($device.Class -eq 'Ports' -and $device.FriendlyName -match '\((COM[0-9]+)\)$') { $portName = $Matches[1] }
        if (-not $dfu -and -not $portName) { continue }
        $paths = @((Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName 'DEVPKEY_Device_LocationPaths' -ErrorAction Stop).Data)
        # Strip only the interface suffix; preserve the controller and hub route.
        $locations = @($paths | Where-Object { $_ -match '#USB\(' } | ForEach-Object { $_ -replace '#USBMI\([0-9]+\)$','' })
        if (-not $locations.Count) { throw "Cannot determine physical USB location: $($device.InstanceId)" }
        [pscustomobject]@{ Id = $device.InstanceId; Port = $portName; Vendor = $vid; Product = $pidValue
            Location = ($locations | Sort-Object | Select-Object -First 1); Dfu = [bool]$dfu }
    }
}

function Select-AutoPort($Devices, [string]$Port, [int]$UsbVendorId, [int]$UsbProductId) {
    if (($UsbVendorId -eq 0) -ne ($UsbProductId -eq 0)) { throw 'Supply both USB IDs or neither.' }
    if (-not $Port -and -not $UsbVendorId) { throw 'Supply -Port, or both -UsbVendorId and -UsbProductId for discovery.' }
    if ($Port -and $Port -notmatch '^COM[0-9]+$') { throw 'Port must be a Windows COM name such as COM7.' }
    $matchesPort = @($Devices | Where-Object {
        $_.Port -and (-not $Port -or $_.Port -eq $Port) -and
        (-not $UsbVendorId -or ($_.Vendor -eq $UsbVendorId -and $_.Product -eq $UsbProductId))
    })
    if ($matchesPort.Count -ne 1) { throw "Expected exactly one matching USB CDC port, found $($matchesPort.Count). Close serial monitors and select the target explicitly." }
    return $matchesPort[0]
}

function Wait-AutoCondition([scriptblock]$Probe, [int]$TimeoutSeconds, [string]$Description) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    do {
        $result = & $Probe
        if ($result) { return $result }
        if ($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds) { break }
        Start-Sleep -Milliseconds 150
    } while ($true)
    throw "Timed out after $TimeoutSeconds seconds: $Description. Use manual factory DFU recovery; no later step was run."
}

function Invoke-AutoTouchPort($Serial) {
    try {
        $Serial.Handshake = [IO.Ports.Handshake]::None
        $Serial.ReadTimeout = 1000; $Serial.WriteTimeout = 1000
        # Windows/.NET initializes DCB with DTR disabled during Open. Establish
        # a live high DTR at a non-triggering baud before arming the firmware.
        $Serial.BaudRate = 115200
        $Serial.DtrEnable = $false
        $Serial.Open()
        $Serial.DtrEnable = $true
        $Serial.BaudRate = 1200
        Start-Sleep -Milliseconds 150
        $Serial.DtrEnable = $false
        $Serial.Close()
    } finally { $Serial.Dispose() }
}

function Invoke-AutoTouch([string]$Port) {
    $serial = New-Object IO.Ports.SerialPort($Port,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
    Invoke-AutoTouchPort $serial
}

function Invoke-AutoUpload {
    param([string]$Firmware, [string]$Port, [int]$UsbVendorId, [int]$UsbProductId,
        [string]$ToolPath, [string]$LegacyDevice = 'Unconfirmed',
        [int]$DfuTimeoutSeconds = 20, [int]$ApplicationTimeoutSeconds = 30)
    if ($LegacyDevice -ne 'AT32UC3B0256') { throw 'Auto-upload currently requires -LegacyDevice AT32UC3B0256 and verified factory DFU 1.0.2.' }
    $stream = $null; $owned = $false
    # Same mutex as the protected uploader. Reentrant acquisition in this thread
    # keeps manual and automatic uploads excluded throughout the touch sequence.
    $mutex = New-Object Threading.Mutex($false, 'Local\UC3BduinoDfuUpload')
    try {
        try { $owned = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $owned = $true }
        if (-not $owned) { throw 'Another UC3Bduino upload is running.' }
        $Firmware = [IO.Path]::GetFullPath($Firmware)
        $stream = [IO.File]::Open($Firmware,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
        Invoke-DfuUpload -Firmware $Firmware -ToolPath $ToolPath -LegacyDevice $LegacyDevice -CheckOnly
        Write-Host '[PASS] Firmware and flasher preflight (no device commands)'
        $devices = @(Get-AutoUsbSnapshot)
        if (@($devices | Where-Object Dfu).Count) { throw 'A DFU device is already present. Use the protected manual uploader or disconnect other targets before auto-upload.' }
        $selected = Select-AutoPort $devices $Port $UsbVendorId $UsbProductId
        Write-Host "Touch target: $($selected.Port), $($selected.Id), $($selected.Location)"
        Invoke-AutoTouch $selected.Port
        Write-Host '[PASS] 1200 baud touch sent (reset is checked next)'
        [void](Wait-AutoCondition {
            @((Get-AutoUsbSnapshot) | Where-Object { $_.Id -eq $selected.Id }).Count -eq 0
        } $DfuTimeoutSeconds 'CDC disappearance after DTR close')
        Write-Host '[PASS] CDC device disappeared'
        $dfu = Wait-AutoCondition {
            $found = @((Get-AutoUsbSnapshot) | Where-Object Dfu)
            if ($found.Count -gt 1) { throw 'Multiple DFU devices appeared; no erase performed.' }
            if ($found.Count -eq 1) {
                if ($found[0].Location -ne $selected.Location) { throw 'DFU appeared at a different physical USB location; no erase performed.' }
                return $found[0]
            }
        } $DfuTimeoutSeconds 'factory DFU 03EB:2FF6 at the selected USB location'
        Write-Host "[PASS] DFU detected: $($dfu.Id)"
        # The existing uploader performs signature/BOOTPROT checks and pins its
        # own bus/address before every erase, flash/readback and launch operation.
        # Check the actual legacy version before allowing any erase in this path.
        $tool = Resolve-DfuTool $ToolPath
        $target = Get-DfuTarget $tool $LegacyDevice
        if ($target.DeviceId -ne $dfu.Id -or
            (Get-DfuValue (Invoke-DfuCommand $tool @($target.Target,'get','bootloader-version'))) -ne 2) {
            throw 'Auto-upload requires verified factory DFU 1.0.2; no erase performed.'
        }
        $now = @((Get-AutoUsbSnapshot) | Where-Object Dfu)
        if ($now.Count -ne 1 -or $now[0].Id -ne $dfu.Id -or $now[0].Location -ne $selected.Location) { throw 'DFU target changed; no erase performed.' }
        Invoke-DfuUpload -Firmware $Firmware -ToolPath $tool -LegacyDevice $LegacyDevice -ExpectedTarget $target
        Write-Host '[PASS] Firmware uploaded with readback; application launch requested'
        $application = Wait-AutoCondition {
            $snapshot = @(Get-AutoUsbSnapshot)
            if (@($snapshot | Where-Object { $_.Dfu -and $_.Location -eq $selected.Location }).Count) { return }
            $found = @($snapshot | Where-Object {
                $_.Port -and $_.Location -eq $selected.Location -and
                $_.Vendor -eq $selected.Vendor -and $_.Product -eq $selected.Product
            })
            if ($found.Count -gt 1) { throw 'Ambiguous CDC interfaces after application launch.' }
            if ($found.Count -eq 1) { return $found[0] }
        } $ApplicationTimeoutSeconds 'application CDC re-enumeration'
        Write-Host "[PASS] Application USB CDC detected: $($application.Port)"
        return $application
    } finally {
        if ($stream) { $stream.Dispose() }
        if ($owned) { $mutex.ReleaseMutex() }
        $mutex.Dispose()
    }
}

function Test-AutoIdentity([string]$Text, [string]$BuildId) {
    $pattern = '(?m)^UC3Bduino AutoFlash Test\r?\nBuild: ' + [regex]::Escape($BuildId) +
        '\r?\nBoard: AT32UC3B0256\r?\nClock: 12 MHz\r?\nUSB CDC: OK\r?$'
    return $Text -match $pattern
}

function Read-AutoIdentity([string]$Port, [string]$BuildId, [int]$TimeoutSeconds, [string]$LogPath) {
    $serial = New-Object IO.Ports.SerialPort($Port,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
    $received = ''
    try {
        $serial.ReadTimeout = 250; $serial.WriteTimeout = 1000
        $serial.DtrEnable = $true
        $serial.Open()
        $timer = [Diagnostics.Stopwatch]::StartNew()
        do {
            $received += $serial.ReadExisting()
            if ($received.Length -gt 65536) { throw 'Unexpectedly large CDC response; identity was not accepted.' }
            if (Test-AutoIdentity $received $BuildId) { return $received }
            Start-Sleep -Milliseconds 100
        } while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds)
        throw "Application identification timed out on $Port (expected build $BuildId)."
    } finally {
        [IO.File]::WriteAllText($LogPath, $received)
        $serial.Dispose()
    }
}
