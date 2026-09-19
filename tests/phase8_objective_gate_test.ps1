$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$verifier = Join-Path $root 'tools\verify-phase8-live-log.ps1'
$completeLog = Join-Path $root 'tests\fixtures\phase8\objective-complete.log'

$completeOutput = @(& $verifier -LogPath $completeLog -FromLine 0 -Require Objective -Json)
$completeResult = ($completeOutput -join [Environment]::NewLine) | ConvertFrom-Json
if (-not $completeResult.Objective.Passed -or
	$completeResult.Objective.PlantLines -ne 1 -or
	$completeResult.Objective.DefuseLines -ne 1) {
	throw 'objective complete fixture did not produce an independent Plant/Defuse gate pass'
}

$selfTestOutput = @(& $verifier -SelfTest -Json)
$selfTestResult = ($selfTestOutput -join [Environment]::NewLine) | ConvertFrom-Json
if (-not $selfTestResult.Objective.Passed) {
	throw 'objective SelfTest did not produce a complete Plant/Defuse gate pass'
}

Write-Output 'phase8 objective gate test: PASS'
