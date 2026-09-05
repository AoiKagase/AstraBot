param(
    [Parameter(Mandatory=$true)][string]$FixtureDirectory,
    [string]$BenchmarkDirectory = '',
    [string]$ManifestPath = ''
)
$ErrorActionPreference = 'Stop'
if (-not $ManifestPath) { $ManifestPath = Join-Path $PSScriptRoot '../tests/nav/fixtures/evidence-manifest.json' }
$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
if ($manifest.schema -ne 1 -or @($manifest.fixtures).Count -ne 10) {
    throw 'Manifest requires schema1 and all ten version/profile fixtures'
}
foreach ($version in 1..5) {
    foreach ($profile in @('minimal', 'full')) {
        $name = "v$version-$profile.nav"
        if (@($manifest.fixtures | Where-Object { $_.name -ceq $name }).Count -ne 1) {
            throw "Manifest missing or duplicate fixture: $name"
        }
    }
}
$records = @($manifest.fixtures)
if ($BenchmarkDirectory) {
    if (@($manifest.benchmarks).Count -ne 2) { throw 'Manifest requires both benchmarks' }
    foreach ($name in @('scene-128.nav', 'scene-1024.nav')) {
        if (@($manifest.benchmarks | Where-Object { $_.name -ceq $name }).Count -ne 1) {
            throw "Manifest missing or duplicate benchmark: $name"
        }
    }
    $records += @($manifest.benchmarks)
}
foreach ($record in $records) {
    $directory = if ($record.name.StartsWith('scene-')) { $BenchmarkDirectory } else { $FixtureDirectory }
    $path = Join-Path $directory $record.name
    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $path).Path)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { $hash = [BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
    if ($bytes.Length -ne $record.bytes -or $hash -cne $record.sha256) {
        throw "Fixture mismatch: $($record.name), length=$($bytes.Length), SHA256=$hash"
    }
    Write-Output "$($record.name) length=$($bytes.Length) SHA256=$hash OK"
}
