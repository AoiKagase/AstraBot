[CmdletBinding()]
param(
    [ValidateSet('PortableDebug', 'MetamodDebug', 'MetamodRelease', 'All')]
    [string]$Profile = 'All',
    [string]$VsDevCmd = '',
    [string]$MetamodSdkRoot = 'H:\sourcecode\003.Game\amxmodx\metamod-p',
    [string]$CacheRoot = ''
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$cacheScript = Join-Path $PSScriptRoot 'verification-cache.ps1'
$expectedSdkSha = '7ec9b014f8c0a947a724644aebe34eb33706e44b'

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

if (-not $VsDevCmd) {
    $localVs = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
    if (Test-Path -LiteralPath $localVs) { $VsDevCmd = $localVs }
    else {
        $locator = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (-not (Test-Path -LiteralPath $locator)) { throw 'vswhere.exe not found' }
        $install = & $locator -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $install) { throw 'MSVC toolchain not found' }
        $VsDevCmd = Join-Path ($install | Select-Object -First 1) 'Common7\Tools\VsDevCmd.bat'
    }
}
if (-not (Test-Path -LiteralPath $VsDevCmd)) { throw "VsDevCmd missing: $VsDevCmd" }
$VsDevCmd = (Resolve-Path -LiteralPath $VsDevCmd).Path

$vsFileHash = Get-FileSha256 -Path $VsDevCmd
$platformIdentity = 'windows-x86-nmake'
$toolchainIdentity = "vsdevcmd-sha256:$vsFileHash;arch=x86;host=x64"

$selectedProfiles = if ($Profile -eq 'All') {
    @('PortableDebug', 'MetamodDebug', 'MetamodRelease')
} else {
    @($Profile)
}

$requiresMetamodSdk = @($selectedProfiles | Where-Object { $_ -ne 'PortableDebug' }).Count -gt 0
if ($requiresMetamodSdk) {
    if (-not (Test-Path -LiteralPath $MetamodSdkRoot -PathType Container)) {
        throw "Metamod-P SDK missing: $MetamodSdkRoot"
    }
    $MetamodSdkRoot = (Resolve-Path -LiteralPath $MetamodSdkRoot).Path
    $sdkSha = (& git -C $MetamodSdkRoot rev-parse HEAD 2>$null).Trim()
    if ($LASTEXITCODE -ne 0 -or $sdkSha -ne $expectedSdkSha) {
        throw "Metamod-P SDK SHA mismatch: expected $expectedSdkSha, got $sdkSha"
    }
    $sdkDirty = & git -C $MetamodSdkRoot status --porcelain 2>$null
    if ($LASTEXITCODE -ne 0 -or $sdkDirty) { throw 'Metamod-P SDK worktree must be clean' }
    $dependencyIdentity = "metamod-p:$expectedSdkSha"
} else {
    $dependencyIdentity = 'none'
}

function Invoke-CanonicalProfile {
    param([Parameter(Mandatory = $true)][string]$SelectedProfile)

    $buildIdentity = switch ($SelectedProfile) {
        'PortableDebug' { 'cmake-debug;metamod=off;tests=on;warnings=on' }
        'MetamodDebug' { 'cmake-debug;metamod=on;tests=on;warnings=on' }
        'MetamodRelease' { 'cmake-release;metamod=on;tests=off;exports=six' }
    }
    $cacheArguments = @{
        Action = 'Check'
        Profile = $SelectedProfile
        RepositoryRoot = $repo
        PlatformIdentity = $platformIdentity
        ToolchainIdentity = $toolchainIdentity
        DependencyIdentity = $dependencyIdentity
        BuildIdentity = $buildIdentity
    }
    if ($CacheRoot) { $cacheArguments.CacheRoot = $CacheRoot }
    $decision = (& $cacheScript @cacheArguments | ConvertFrom-Json)
    if (-not $decision.shouldRun) {
        Write-Output "SKIP $SelectedProfile ($($decision.reason)); fingerprint=$($decision.fingerprint); verifiedTree=$($decision.treeSha)"
        return
    }

    $sdkArgument = '-DASTRABOT_METAMOD_SDK_ROOT="' + $MetamodSdkRoot + '"'
    $command = switch ($SelectedProfile) {
        'PortableDebug' {
            'call "' + $VsDevCmd + '" -arch=x86 -host_arch=x64 && ' +
                'cmake -S . -B build-portable-x86-test -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug ' +
                '-DASTRABOT_BUILD_METAMOD=OFF -DASTRABOT_BUILD_TESTS=ON ' +
                '-DASTRABOT_WARNINGS_AS_ERRORS=ON && ' +
                'cmake --build build-portable-x86-test && ' +
                'ctest --test-dir build-portable-x86-test --output-on-failure'
        }
        'MetamodDebug' {
            'call "' + $VsDevCmd + '" -arch=x86 -host_arch=x64 && ' +
                'cmake -S . -B build-metamod-x86-test -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug ' +
                '-DASTRABOT_BUILD_METAMOD=ON -DASTRABOT_BUILD_TESTS=ON ' +
                '-DASTRABOT_WARNINGS_AS_ERRORS=ON ' + $sdkArgument + ' && ' +
                'cmake --build build-metamod-x86-test && ' +
                'ctest --test-dir build-metamod-x86-test --output-on-failure'
        }
        'MetamodRelease' {
            'call "' + $VsDevCmd + '" -arch=x86 -host_arch=x64 && ' +
                'cmake -S . -B build-metamod-x86-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ' +
                '-DASTRABOT_BUILD_METAMOD=ON -DASTRABOT_BUILD_TESTS=OFF ' +
                '-DASTRABOT_WARNINGS_AS_ERRORS=ON ' + $sdkArgument + ' && ' +
                'cmake --build build-metamod-x86-release && ' +
                'dumpbin /exports build-metamod-x86-release\astrabot_mm.dll'
        }
    }

    Write-Output "RUN $SelectedProfile; fingerprint=$($decision.fingerprint)"
    $recordArguments = @{
        Action = 'Record'
        Profile = $SelectedProfile
        RepositoryRoot = $repo
        PlatformIdentity = $platformIdentity
        ToolchainIdentity = $toolchainIdentity
        DependencyIdentity = $dependencyIdentity
        BuildIdentity = $buildIdentity
        Result = 'Passed'
        Command = $command
    }
    if ($CacheRoot) { $recordArguments.CacheRoot = $CacheRoot }
    Push-Location $repo
    try {
        $output = & cmd /d /c $command 2>&1
        $output | Write-Output
        if ($LASTEXITCODE -ne 0) { throw "Canonical verification failed ($SelectedProfile): $LASTEXITCODE" }

        if ($SelectedProfile -eq 'MetamodRelease') {
            $expectedExports = @('Meta_Query', 'Meta_Attach', 'Meta_Detach', 'GetEntityAPI2', 'GetEngineFunctions', 'GiveFnptrsToDll')
            $exportNames = @(
                [regex]::Matches(($output -join "`n"), '(?m)^\s*\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)\s*$') |
                    ForEach-Object { $_.Groups[1].Value }
            )
            if (@(Compare-Object -CaseSensitive $expectedExports $exportNames).Count -ne 0) {
                throw "Unexpected adapter exports: $($exportNames -join ', ')"
            }
        }
    } catch {
        $recordArguments.Result = 'Failed'
        try { & $cacheScript @recordArguments | Out-Null }
        catch { Write-Warning "Could not record failed verification: $($_.Exception.Message)" }
        throw
    } finally { Pop-Location }

    & $cacheScript @recordArguments | Out-Null
    Write-Output "PASS $SelectedProfile; recorded verified HEAD/tree and fingerprint $($decision.fingerprint)"
}

foreach ($selectedProfile in $selectedProfiles) {
    Invoke-CanonicalProfile -SelectedProfile $selectedProfile
}
