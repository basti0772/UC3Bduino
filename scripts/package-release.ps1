# SPDX-License-Identifier: MIT
#Requires -Version 5.1
[CmdletBinding()]
param([string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
try {
    $root = Split-Path -Parent $PSScriptRoot
    $version = [IO.File]::ReadAllText((Join-Path $root 'VERSION')).Trim()
    if ($version -cnotmatch '^0\.[0-9]+\.[0-9]+-(dev|rc[1-9][0-9]*)$') {
        throw 'This packager prepares development and RC source archives only. Stable releases require the documented hardware/release review.'
    }
    $platform = [IO.File]::ReadAllText((Join-Path $root 'arduino/hardware/uc3bduino/avr32/platform.txt'))
    if ($platform -notmatch ('(?m)^version=' + [regex]::Escape($version) + '\r?$')) { throw 'VERSION and platform.txt disagree.' }
    $library = [IO.File]::ReadAllText((Join-Path $root 'arduino/hardware/uc3bduino/avr32/libraries/UC3BduinoExamples/library.properties'))
    if ($library -notmatch ('(?m)^version=' + [regex]::Escape($version) + '\r?$')) { throw 'VERSION and library.properties disagree.' }
    $header = [IO.File]::ReadAllText((Join-Path $root 'arduino/hardware/uc3bduino/avr32/cores/uc3bduino/UC3BduinoVersion.h'))
    if (-not $header.Contains('#define UC3BDUINO_VERSION "' + $version + '"')) { throw 'VERSION and UC3BduinoVersion.h disagree.' }
    $paths = [IO.File]::ReadAllLines((Join-Path $root 'packaging/source-files.txt'))
    $seen = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    $sources = foreach ($relative in $paths) {
        # Explicit, reviewed allowlist; never recurse over arbitrary working-tree files.
        if ($relative -notmatch '^(?:[A-Za-z0-9_.-]+/)*[A-Za-z0-9_.-]+$' -or
            $relative -match '(^|/)(\.\.?|\.git|\.scratch|build|dist)(/|$)|\.local\.txt$|\.(exe|dll|o|elf|hex|bin|a|zip|pyc)$') {
            throw "Disallowed source archive entry: '$relative'"
        }
        if (-not $seen.Add($relative)) { throw "Duplicate archive entry: $relative" }
        $full = Join-Path $root $relative
        if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { throw "Missing source file: $relative" }
        $ancestor = $full
        while ($ancestor -and $ancestor -ne $root) {
            if (((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Source contains a reparse point: $relative"
            }
            $ancestor = Split-Path -Parent $ancestor
        }
        [pscustomobject]@{ Relative = $relative; Full = $full }
    }
    if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root "dist/$version" }
    $output = [IO.Path]::GetFullPath($OutputDirectory)
    if (Test-Path -LiteralPath $output) { throw "Output already exists: $output. Choose a new directory; no files are overwritten." }
    [void](New-Item -ItemType Directory -Path $output)
    Add-Type -AssemblyName System.IO.Compression
    $prefix = "UC3Bduino-$version"
    $zipPath = Join-Path $output "$prefix.zip"
    $fileStream = [IO.File]::Open($zipPath, [IO.FileMode]::CreateNew)
    $zip = New-Object IO.Compression.ZipArchive($fileStream, [IO.Compression.ZipArchiveMode]::Create)
    $fileHashes = New-Object 'System.Collections.Generic.List[string]'
    $sha = [Security.Cryptography.SHA256]::Create()
    $utf8 = New-Object Text.UTF8Encoding($false)
    try {
        foreach ($source in ($sources | Sort-Object Relative)) {
            $bytes = [IO.File]::ReadAllBytes($source.Full)
            $hash = [BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-','').ToLowerInvariant()
            $fileHashes.Add("$hash  $($source.Relative)")
            $entry = $zip.CreateEntry("$prefix/$($source.Relative)", [IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = [DateTimeOffset]::new(2000,1,1,0,0,0,[TimeSpan]::Zero)
            $stream = $entry.Open()
            try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
        }
        $entry = $zip.CreateEntry("$prefix/SOURCE_SHA256SUMS.txt")
        $entry.LastWriteTime = [DateTimeOffset]::new(2000,1,1,0,0,0,[TimeSpan]::Zero)
        $bytes = $utf8.GetBytes(($fileHashes -join "`n") + "`n")
        $stream = $entry.Open()
        try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
    } finally { $sha.Dispose(); $zip.Dispose(); $fileStream.Dispose() }
    Copy-Item -LiteralPath (Join-Path $root 'docs/INSTALL_WINDOWS.md') -Destination $output
    $checksums = foreach ($file in @($zipPath, (Join-Path $output 'INSTALL_WINDOWS.md'))) {
        (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + [IO.Path]::GetFileName($file)
    }
    [IO.File]::WriteAllText((Join-Path $output 'SHA256SUMS.txt'), ($checksums -join "`n") + "`n", $utf8)
    Write-Host "Development source archive: $zipPath"
    Write-Host 'Documentation and SHA256SUMS.txt are beside the archive. No tools, local configuration, tag or GitHub release are created.'
} catch { Write-Error -ErrorAction Continue $_.Exception.Message; exit 1 }
