[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Check', 'Record')]
    [string]$Action,

    [Parameter(Mandatory = $true)]
    [ValidateSet('PortableDebug', 'MetamodDebug', 'MetamodRelease', 'PortableEvidence', 'WindowsAsanEvidence', 'LinuxPortableDebug')]
    [string]$Profile,

    [string]$RepositoryRoot = '',
    [string]$CacheRoot = '',
    [string]$PlatformIdentity = '',
    [string]$ToolchainIdentity = '',
    [string]$DependencyIdentity = '',
    [string]$BuildIdentity = '',
    [ValidateSet('Passed', 'Failed')]
    [string]$Result = 'Passed',
    [string]$Command = ''
)

$ErrorActionPreference = 'Stop'

if (-not $RepositoryRoot) {
    $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
}

if (-not $CacheRoot) {
    if ($env:ASTRABOT_VERIFICATION_CACHE) {
        $CacheRoot = $env:ASTRABOT_VERIFICATION_CACHE
    } elseif ($env:LOCALAPPDATA) {
        $CacheRoot = Join-Path $env:LOCALAPPDATA 'AstraBot\verification-cache'
    } else {
        $CacheRoot = Join-Path $env:TEMP 'AstraBot\verification-cache'
    }
}
if (-not [IO.Path]::IsPathRooted($CacheRoot)) {
    $CacheRoot = Join-Path $RepositoryRoot $CacheRoot
}
$CacheRoot = [IO.Path]::GetFullPath($CacheRoot)

function Get-GitValue {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)

    $value = & git -C $RepositoryRoot @Arguments 2>$null
    if ($LASTEXITCODE -ne 0) { return 'unavailable' }
    return (($value -join "`n").Trim())
}

function Get-FileSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    $sha = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($Path)
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant()
    } finally {
        $stream.Dispose()
        $sha.Dispose()
    }
}

function Get-ContentFingerprint {
    param([Parameter(Mandatory = $true)][string[]]$Paths)

    $entries = [Collections.Generic.List[string]]::new()
    foreach ($relativePath in $Paths) {
        $absolutePath = Join-Path $RepositoryRoot ($relativePath.Replace('/', [IO.Path]::DirectorySeparatorChar))
        if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
            $entries.Add("$relativePath`t<missing>")
            continue
        }
        $fileHash = Get-FileSha256 -Path $absolutePath
        $entries.Add("$relativePath`t$fileHash")
    }

    $manifest = @(
        'astrabot-verification-inputs-v1'
        "profile=$Profile"
        "platform=$PlatformIdentity"
        "toolchain=$ToolchainIdentity"
        "dependency=$DependencyIdentity"
        "build=$BuildIdentity"
        'files:'
        $entries
    ) -join "`n"
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString(
            $sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($manifest))
        )).Replace('-', '').ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
}

# Include source, test, tool, CMake, dependency and CI inputs. Documentation,
# planning state, generated build output and local indexes are intentionally not
# included, so docs-only edits do not invalidate a source verification result.
$gitFiles = Get-GitValue -Arguments @('ls-files', '-co', '--exclude-standard')
$candidatePaths = @($gitFiles -split "`r?`n" |
    Where-Object { $_ })
$inputPaths = @(
    $candidatePaths |
        ForEach-Object { $_.Replace('\', '/') } |
        Where-Object {
            $_ -match '^(src|include|tests|tools|cmake)/' -or
            $_ -match '^\.github/(workflows|actions)/' -or
            $_ -match '^(CMakeLists\.txt|CMakePresets\.json|CMakeSettings\.json|vcpkg\.json|conanfile(?:\.(txt|py))?|package(?:-lock)?\.json|pnpm-lock\.yaml|Cargo\.(toml|lock)|go\.(mod|sum)|Directory\.Build\.(props|targets)|[^/]+\.(cmake|lock|toml))$'
        } |
        Sort-Object -Unique
)
if ($inputPaths.Count -eq 0) { throw 'No build inputs found' }

$headSha = Get-GitValue -Arguments @('rev-parse', 'HEAD')
$treeSha = Get-GitValue -Arguments @('rev-parse', 'HEAD^{tree}')
$fingerprint = Get-ContentFingerprint -Paths $inputPaths
$recordDirectory = Join-Path $CacheRoot $Profile
$recordPath = Join-Path $recordDirectory "$fingerprint.json"

if ($Action -eq 'Check') {
    $record = $null
    if (Test-Path -LiteralPath $recordPath -PathType Leaf) {
        try {
            $record = Get-Content -Raw -LiteralPath $recordPath | ConvertFrom-Json
        } catch {
            $record = $null
        }
    }

    $reusable = $null -ne $record -and
        $record.result -eq 'passed' -and
        $record.fingerprint -eq $fingerprint -and
        $record.buildIdentity -eq $BuildIdentity -and
        $record.platformIdentity -eq $PlatformIdentity -and
        $record.toolchainIdentity -eq $ToolchainIdentity -and
        $record.dependencyIdentity -eq $DependencyIdentity
    $reason = if ($reusable) { 'successful-input-match' }
              elseif ($record) { 'record-metadata-mismatch' }
              else { 'no-success-record' }

    [ordered]@{
        shouldRun = -not $reusable
        reason = $reason
        profile = $Profile
        fingerprint = $fingerprint
        headSha = $headSha
        treeSha = $treeSha
        inputCount = $inputPaths.Count
        cachePath = $recordPath
    } | ConvertTo-Json -Depth 4
    exit 0
}

if (-not (Test-Path -LiteralPath $recordDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $recordDirectory -Force | Out-Null
}

$record = [ordered]@{
    schema = 1
    result = $Result.ToLowerInvariant()
    profile = $Profile
    fingerprint = $fingerprint
    headSha = $headSha
    treeSha = $treeSha
    repositoryRoot = $RepositoryRoot
    platformIdentity = $PlatformIdentity
    toolchainIdentity = $ToolchainIdentity
    dependencyIdentity = $DependencyIdentity
    buildIdentity = $BuildIdentity
    inputCount = $inputPaths.Count
    command = $Command
    recordedAtUtc = [DateTime]::UtcNow.ToString('o')
}
[IO.File]::WriteAllText(
    $recordPath,
    ($record | ConvertTo-Json -Depth 6),
    [Text.UTF8Encoding]::new($false)
)
$record | ConvertTo-Json -Depth 6
