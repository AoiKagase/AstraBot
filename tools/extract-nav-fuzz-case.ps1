param(
    [Parameter(Mandatory=$true)][string]$Journal,
    [Parameter(Mandatory=$true)][string]$OutputFile
)
$ErrorActionPreference = 'Stop'
# Explicit output path; never replace an existing file.
$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Journal).Path)
if ($bytes.Length -lt 4) { throw 'Truncated journal length' }
$length = [uint64]$bytes[0] + ([uint64]$bytes[1] * 256) +
    ([uint64]$bytes[2] * 65536) + ([uint64]$bytes[3] * 16777216)
if ($length -gt 65536 -or $length -gt $bytes.Length - 4) { throw 'Invalid journal length' }
$stream = [System.IO.File]::Open($OutputFile, [System.IO.FileMode]::CreateNew)
try { $stream.Write($bytes, 4, [int]$length) } finally { $stream.Dispose() }
Write-Output "Extracted $length bytes to $OutputFile"
