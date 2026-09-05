param([Parameter(Mandatory=$true)][string]$FixtureDirectory,
      [Parameter(Mandatory=$true)][string]$ArtifactDirectory)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$checker = Join-Path $repo 'tools/check-nav-manifest.ps1'
$source = Get-Content -LiteralPath (Join-Path $repo 'tests/nav/fixtures/evidence-manifest.json') -Raw
$null = New-Item -ItemType Directory -Path $ArtifactDirectory -Force
foreach ($case in @('empty', 'hash', 'duplicate', 'missing')) {
    $manifest = $source | ConvertFrom-Json
    switch ($case) {
        empty { $manifest.fixtures = @() }
        hash { $manifest.fixtures[0].sha256 = 'deliberately-wrong' }
        duplicate { $manifest.fixtures[0].name = $manifest.fixtures[1].name }
        missing { $manifest.fixtures = @($manifest.fixtures | Select-Object -Skip 1) }
    }
    $path = Join-Path (Resolve-Path -LiteralPath $ArtifactDirectory).Path "$case.json"
    [System.IO.File]::WriteAllText($path, ($manifest | ConvertTo-Json -Depth 10))
    $rejected = $false
    try { & $checker -FixtureDirectory $FixtureDirectory -ManifestPath $path | Out-Null }
    catch { $rejected = $true }
    if (-not $rejected) { throw "Invalid manifest accepted: $case" }
    Write-Output "manifest regression: $case rejected"
}
