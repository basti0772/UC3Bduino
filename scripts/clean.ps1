# SPDX-License-Identifier: MIT
#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess = $true)]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$buildPath = [IO.Path]::GetFullPath((Join-Path $repoRoot 'build'))
$expectedPath = $repoRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar + 'build'
if ($buildPath -ne $expectedPath) {
    throw "Refusing to remove an unexpected build path: $buildPath"
}
if (Test-Path -LiteralPath $buildPath) {
    # Refuse junctions/symlinks anywhere in the tree before recursive deletion.
    $pending = New-Object 'System.Collections.Generic.Stack[string]'
    $pending.Push($buildPath)
    while ($pending.Count -gt 0) {
        $item = Get-Item -LiteralPath $pending.Pop() -Force
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Refusing to clean a symlink or junction: $($item.FullName)"
        }
        if ($item.PSIsContainer) {
            foreach ($child in Get-ChildItem -LiteralPath $item.FullName -Force) {
                $pending.Push($child.FullName)
            }
        }
    }
    if ($PSCmdlet.ShouldProcess($buildPath, 'Remove generated build output')) {
        Remove-Item -LiteralPath $buildPath -Recurse -Force
    }
}
