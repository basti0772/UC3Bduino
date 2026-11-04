# SPDX-License-Identifier: MIT
#Requires -Version 5.1
# Synthetic host lifecycle; never opens a COM port or calls a real flasher.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../scripts/dfu-common.ps1')
. (Join-Path $PSScriptRoot '../tools/auto-upload-common.ps1')
$script:checks = 0
function Assert($Condition, [string]$Name) {
    if (-not $Condition) { throw "FAIL: $Name" }
    ++$script:checks; Write-Host "PASS: $Name"
}
function Reject([scriptblock]$Action, [string]$Pattern) {
    $message = ''
    try { & $Action | Out-Null } catch { $message = $_.Exception.Message }
    Assert ($message -match $Pattern) "reject $Pattern (received: $message)"
}
$script:touchTrace = New-Object 'Collections.Generic.List[string]'
$touchPort = [pscustomobject]@{Handshake=0; ReadTimeout=0; WriteTimeout=0}
$touchPort | Add-Member ScriptProperty BaudRate { 0 } { param($value) $script:touchTrace.Add("baud=$value") }
$touchPort | Add-Member ScriptProperty DtrEnable { $false } { param($value) $script:touchTrace.Add("dtr=$value") }
$touchPort | Add-Member ScriptMethod Open { $script:touchTrace.Add('open') }
$touchPort | Add-Member ScriptMethod Close { $script:touchTrace.Add('close') }
$touchPort | Add-Member ScriptMethod Dispose { $script:touchTrace.Add('dispose') }
Invoke-AutoTouchPort $touchPort
Assert (($script:touchTrace -join ',') -eq 'baud=115200,dtr=False,open,dtr=True,baud=1200,dtr=False,close,dispose') 'touch opens disarmed, establishes DTR, arms 1200, then closes'
$script:touchTrace.Clear()
$touchPort | Add-Member ScriptMethod Open { throw 'synthetic open failure' } -Force
Reject { Invoke-AutoTouchPort $touchPort } 'synthetic open failure'
Assert (($script:touchTrace -join ',') -eq 'baud=115200,dtr=False,dispose') 'failed open disposes without arming touch'
function Get-PnpDevice {
    param([switch]$PresentOnly, $ErrorAction)
    [pscustomobject]@{InstanceId='USB\VID_FFFF&PID_FFFE\TEST';Class='Ports';FriendlyName='Test (COM7)'}
    [pscustomobject]@{InstanceId='USB\VID_03EB&PID_2FF6\1.0.2';Class='USB';FriendlyName='DFU'}
    [pscustomobject]@{InstanceId='BTH\NON_USB';Class='Ports';FriendlyName='Other (COM8)'}
}
function Get-PnpDeviceProperty {
    param($InstanceId, $KeyName, $ErrorAction)
    [pscustomobject]@{Data=@('PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(3)')}
}
$parsed = @(Get-AutoUsbSnapshot)
Assert ($parsed.Count -eq 2 -and $parsed[0].Port -eq 'COM7' -and $parsed[0].Vendor -eq 65535 -and $parsed[1].Dfu) 'PnP parsing excludes non-USB COM devices'
$cdc = $parsed[0]; $dfu = $parsed[1]
Assert ((Select-AutoPort @($cdc) 'COM7' 0 0).Id -eq $cdc.Id) 'explicit USB port'
Assert ((Select-AutoPort @($cdc) '' 65535 65534).Port -eq 'COM7') 'VID/PID discovery'
Reject { Select-AutoPort @($cdc) '' 0 0 } 'Supply -Port'
Reject { Select-AutoPort @($cdc) '' 65535 0 } 'both USB IDs'
Reject { Select-AutoPort @($cdc) '\\.\COM7' 0 0 } 'Port must'
Reject { Select-AutoPort @($cdc,$cdc) '' 65535 65534 } 'exactly one'
Reject { Select-AutoPort @($cdc) 'COM7' 123 456 } 'exactly one'
$identity = "UC3Bduino AutoFlash Test`r`nBuild: UNIQUE`r`nBoard: AT32UC3B0256`r`nClock: 12 MHz`r`nUSB CDC: OK`r`n"
Assert (Test-AutoIdentity $identity 'UNIQUE') 'complete build/board/clock identity'
Assert (-not (Test-AutoIdentity $identity 'OLD')) 'old firmware identity rejected'
Assert (-not (Test-AutoIdentity ($identity.Replace('12 MHz','16 MHz')) 'UNIQUE')) 'wrong clock identity rejected'
Assert (-not (Test-AutoIdentity ($identity.Replace('0256','0512')) 'UNIQUE')) 'wrong MCU identity rejected'

function Get-AutoUsbSnapshot {
    if (-not $script:touched) {
        $cdc
        if ($script:scenario -eq 'already-dfu') { $dfu }
    } elseif ($script:scenario -eq 'no-reset') { $cdc }
    elseif ($script:launched) {
        if ($script:scenario -eq 'no-app') { $dfu }
        else { [pscustomobject]@{Id=$cdc.Id;Port='COM9';Vendor=$cdc.Vendor;Product=$cdc.Product;Location=$cdc.Location;Dfu=$false} }
    } elseif ($script:scenario -ne 'no-dfu') {
        if ($script:scenario -eq 'wrong-location') {
            [pscustomobject]@{Id=$dfu.Id;Port='';Vendor=1003;Product=12278;Location='OTHER';Dfu=$true}
        } else { $dfu }
        if ($script:scenario -eq 'multiple-dfu') { $dfu }
    }
}
function Invoke-AutoTouch([string]$Port) {
    Assert ($Port -eq 'COM7') 'touch selected port'
    $script:touched = $true
    # Production must keep the validated image immutable during reboot too.
    $blocked = $false
    try { $writer = [IO.File]::OpenWrite($firmware); $writer.Dispose() } catch [IO.IOException] { $blocked = $true }
    Assert $blocked 'firmware write lock spans touch'
}
function Resolve-DfuTool([string]$ToolPath) { 'synthetic-tool' }
function Get-DfuTarget([string]$Tool, [string]$LegacyDevice) { @{Target='at32uc3b0256:1,2';DeviceId=$dfu.Id} }
function Invoke-DfuCommand([string]$Tool, [string[]]$Arguments) {
    if ($script:scenario -eq 'wrong-boot') { '0x03 (3)' } else { '0x02 (2)' }
}
function Invoke-DfuUpload {
    param($Firmware,$ToolPath,$LegacyDevice,[switch]$CheckOnly,$ExpectedTarget)
    if ($CheckOnly) {
        [void](Test-DfuHex ([IO.File]::ReadAllText($Firmware)))
        if ($script:scenario -eq 'bad-tool') { throw 'Unsupported upload tool version' }
    } else {
        Assert ($ExpectedTarget.Target -eq 'at32uc3b0256:1,2') 'pass pinned target to protected uploader'
        $script:uploads++
        if ($script:scenario -eq 'flash-fail') {
            $failure = New-Object Exception('synthetic flash failure'); $failure.Data['ExitCode'] = 7; throw $failure
        }
        $script:launched = $true
    }
}
function Run([string]$Scenario) {
    $script:scenario = $Scenario; $script:touched = $false; $script:launched = $false; $script:uploads = 0
    Invoke-AutoUpload -Firmware $firmware -Port COM7 -ToolPath synthetic -LegacyDevice AT32UC3B0256 -DfuTimeoutSeconds 1 -ApplicationTimeoutSeconds 1
}
$directory = Join-Path (Split-Path -Parent $PSScriptRoot) ('.scratch/auto-tests-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
$firmware = Join-Path $directory 'firmware with spaces.hex'
[IO.File]::WriteAllText($firmware, ":0200000480007A`n:04200000D703000002`n:00000001FF`n")
$result = Run 'success'
Assert ($result.Port -eq 'COM9' -and $script:uploads -eq 1 -and $script:launched) 'complete lifecycle accepts changed COM number'
foreach ($case in @(
    @('bad-tool','Unsupported'), @('already-dfu','already present'), @('no-reset','CDC disappearance'),
    @('no-dfu','factory DFU'), @('wrong-location','different physical'), @('multiple-dfu','Multiple DFU'), @('wrong-boot','1.0.2'))) {
    Reject { Run $case[0] } $case[1]
    Assert ($script:uploads -eq 0) "no upload after $($case[0])"
    if ($case[0] -in @('bad-tool','already-dfu')) { Assert (-not $script:touched) 'preflight failure prevents touch' }
}
$caughtCode = 0
try { Run 'flash-fail' | Out-Null } catch { $caughtCode = $_.Exception.Data['ExitCode'] }
Assert ($caughtCode -eq 7 -and -not $script:launched) 'flasher failure propagates without application launch'
Reject { Run 'no-app' } 'application CDC'
Assert ($script:uploads -eq 1) 'missing application is not reported as successful cycle'
[IO.File]::WriteAllText($firmware, 'invalid')
Reject { Run 'success' } 'Malformed'
Assert (-not $script:touched) 'bad firmware never touches board'
Write-Host "PASS: $script:checks automatic-upload host checks (synthetic; no hardware)"
