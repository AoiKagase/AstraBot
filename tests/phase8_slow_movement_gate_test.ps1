$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$verifier = Join-Path $root 'tools\verify-phase8-live-log.ps1'
$fixture = Join-Path $root 'tests\fixtures\phase8\slow-movement.log'
& $verifier -LogPath $fixture -Require Movement
if ($LASTEXITCODE -ne 0) {
	exit $LASTEXITCODE
}
Write-Output 'phase8_slow_movement_gate_test: PASS'
