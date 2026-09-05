param(
    [ValidateSet('Debug', 'Analyze', 'Asan')][string]$Mode = 'Debug',
    [string]$VsDevCmd = '',
    [switch]$LongFuzz
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $VsDevCmd) {
    $localVs = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
    if (Test-Path -LiteralPath $localVs) { $VsDevCmd = $localVs }
    else {
        $locator = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        $install = & $locator -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $install) { throw 'MSVC toolchain not found' }
        $VsDevCmd = Join-Path $install 'Common7\Tools\VsDevCmd.bat'
    }
}
if (-not (Test-Path -LiteralPath $VsDevCmd)) { throw 'VsDevCmd missing' }
$directory = switch ($Mode) {
    Debug { 'build-portable-test' }
    Analyze { 'build-portable-analyze' }
    Asan { 'build-nav-asan' }
}
$extra = '-DASTRABOT_NAV_ASAN=OFF'
if ($Mode -eq 'Analyze') { $extra += ' "-DCMAKE_CXX_FLAGS=/EHsc /analyze"' }
if ($Mode -eq 'Asan') { $extra = '-DASTRABOT_NAV_ASAN=ON' }
Push-Location $repo
try {
    $command = 'call "' + $VsDevCmd + '" -arch=x64 -host_arch=x64 && ' +
        'cmake -S . -B ' + $directory + ' -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug ' +
        '-DASTRABOT_BUILD_METAMOD=OFF -DASTRABOT_BUILD_TESTS=ON ' +
        '-DASTRABOT_WARNINGS_AS_ERRORS=ON -DASTRABOT_BUILD_NAV_BENCHMARK=ON ' + $extra +
        ' && cmake --build ' + $directory +
        ' && ctest --test-dir ' + $directory + ' --output-on-failure' +
        ' && ' + $directory + '\astrabot_nav_corruption_tests.exe ' + $directory + '\evidence-fixtures'
    if ($LongFuzz) { $command += ' && ctest --test-dir ' + $directory + '/long-fuzz --output-on-failure' }
    & cmd /d /c $command
    if ($LASTEXITCODE -ne 0) { throw "Verification failed ($Mode): $LASTEXITCODE" }
    & (Join-Path $PSScriptRoot 'check-nav-manifest.ps1') -FixtureDirectory (Join-Path $repo "$directory/evidence-fixtures")
    & (Join-Path $repo 'tests/nav/manifest_tests.ps1') -FixtureDirectory (Join-Path $repo "$directory/evidence-fixtures") -ArtifactDirectory (Join-Path $repo "$directory/manifest-selftest")
} finally { Pop-Location }
