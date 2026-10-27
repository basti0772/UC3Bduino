# SPDX-License-Identifier: MIT
#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess = $true)]
param([string]$Destination = (Join-Path $env:LOCALAPPDATA 'UC3Bduino/tools/dfu-programmer-1.1.0'),
      [string]$ArchivePath)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
try {
    $destinationPath = [IO.Path]::GetFullPath($Destination)
    if (Test-Path -LiteralPath $destinationPath) { throw "Destination exists: $destinationPath. Nothing was changed." }
    $uri = 'https://github.com/dfu-programmer/dfu-programmer/releases/download/v1.1.0/dfu-programmer-x64-1.1.0.zip'
    $sha256 = '0ab135fd9f7c54d142fdd9112f2183d82f5a362032dc3d52860a84756fe96e27'
    if (-not $PSCmdlet.ShouldProcess($destinationPath, 'Download/verify/extract the complete upstream dfu-programmer 1.1.0 x64 ZIP (no execution or driver installation)')) { return }
    if (-not $ArchivePath) {
        $ArchivePath = Join-Path ([IO.Path]::GetTempPath()) ('UC3Bduino-dfu-' + [guid]::NewGuid().ToString('N') + '.zip')
        # Enable TLS 1.2 for Windows PowerShell without changing persistent settings.
        $oldTls = [Net.ServicePointManager]::SecurityProtocol
        try {
            [Net.ServicePointManager]::SecurityProtocol = $oldTls -bor [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -UseBasicParsing -Uri $uri -OutFile $ArchivePath -TimeoutSec 120
        } finally { [Net.ServicePointManager]::SecurityProtocol = $oldTls }
        Write-Host "Downloaded archive retained at $ArchivePath"
    }
    if ((Get-Item -LiteralPath $ArchivePath).Length -ne 423421 -or
        (Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha256) {
        throw 'DFU archive size/SHA-256 mismatch. Nothing extracted; do not execute the download.'
    }
    Expand-Archive -LiteralPath $ArchivePath -DestinationPath $destinationPath
    $tools = @(Get-ChildItem -LiteralPath $destinationPath -Filter dfu-programmer.exe -Recurse -File)
    if ($tools.Count -ne 1) { throw 'Unexpected upstream ZIP layout; executable not uniquely identified.' }
    Write-Host 'Verified upstream distribution extracted, including DLLs and licenses. No executable was run.'
    Write-Host ('For this shell: $env:UC3B_DFU_TOOL_PATH = ''' + $tools[0].DirectoryName.Replace("'", "''") + "'")
} catch { Write-Error -ErrorAction Continue $_.Exception.Message; exit 1 }
