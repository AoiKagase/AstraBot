$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$verifier = Join-Path $root 'tools\verify-phase8-live-log.ps1'
& $verifier -SelfTest
if ($LASTEXITCODE -ne 0) {
	exit $LASTEXITCODE
}
& (Join-Path $root 'tests\phase8_objective_gate_test.ps1')
Write-Output 'phase8_live_log_tests: PASS'
