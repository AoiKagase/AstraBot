[CmdletBinding()]
param(
	[Parameter(Mandatory = $false)]
	[string]$LogPath,
	[Parameter(Mandatory = $false)]
	[int]$FromLine = 0,
	[Parameter(Mandatory = $false)]
	[ValidateSet('Movement', 'Combat', 'C4', 'Objective')]
	[string[]]$Require = @('Movement', 'Combat', 'Objective'),
	[switch]$SelfTest,
	[switch]$Json
)

$ErrorActionPreference = 'Stop'

function Assert-Condition {
	param([bool]$Condition, [string]$Message)
	if (-not $Condition) {
		throw "self-test failed: $Message"
	}
}

function Parse-MovementSample {
	param([string]$Line)
	$pattern = 'movement physics actor=(?<actor>\d+) generation=(?<generation>\d+) frame=(?<frame>\d+).*?dispatched=(?<dispatched>\d+).*?before=\((?<bx>-?[\d.]+) (?<by>-?[\d.]+) (?<bz>-?[\d.]+)\) after=\((?<ax>-?[\d.]+) (?<ay>-?[\d.]+) (?<az>-?[\d.]+)\).*?deadflag=(?<deadflag>-?\d+) team=(?<team>-?\d+)(?: teamConfirmed=(?<teamConfirmed>\d+))?.*?ready=(?<ready>\d+)'
	$match = [regex]::Match($Line, $pattern)
	if (-not $match.Success) {
		return $null
	}
	[pscustomobject]@{
		Actor = [int]$match.Groups['actor'].Value
		Generation = [int]$match.Groups['generation'].Value
		Frame = [int]$match.Groups['frame'].Value
		Dispatched = [int]$match.Groups['dispatched'].Value
		Deadflag = [int]$match.Groups['deadflag'].Value
		Team = [int]$match.Groups['team'].Value
		TeamConfirmed = if ($match.Groups['teamConfirmed'].Success) { [int]$match.Groups['teamConfirmed'].Value } else { 0 }
		Ready = [int]$match.Groups['ready'].Value
		AfterX = [double]$match.Groups['ax'].Value
		AfterY = [double]$match.Groups['ay'].Value
		Line = $Line
	}
}

function Get-LiveLogResult {
	param([System.Collections.IEnumerable]$Lines, [int]$SkipLines = 0)
	$windows = @{}
	$moving = @{}
	$movementEvidence = [System.Collections.Generic.List[string]]::new()
	$attackEvidence = [System.Collections.Generic.List[string]]::new()
	$plantEvidence = [System.Collections.Generic.List[string]]::new()
	$defuseEvidence = [System.Collections.Generic.List[string]]::new()
	$attackPattern = [regex]::new('"[^"\r\n]*<BOT>[^"\r\n]*"\s+attacked\s+"[^"\r\n]*<BOT>[^"\r\n]*"', 'Compiled')
	$scopePattern = [regex]::new('(?:^| )(map|round)=(\d+)', 'Compiled')
	$readyCount = 0; $idleCount = 0; $attackCount = 0; $plantCount = 0; $defuseCount = 0
	$totalLines = 0; $logLines = 0
	foreach ($line in $Lines) {
		++$totalLines
		if ($totalLines -le $SkipLines) { continue }
		++$logLines
		if ($line.Contains('triggered "Round_Start"') -or $line.Contains('Loading map "') -or
			$line.StartsWith('Protocol version ')) { $windows.Clear() }
		if ($line.Contains('Game_idle_kick')) { ++$idleCount }
		if ($line.StartsWith('movement physics ')) {
			$sample = Parse-MovementSample $line
			if ($null -eq $sample) { continue }
			$actor = $sample.Actor
			$scope = "$($sample.Generation)"
			foreach ($match in $scopePattern.Matches($line)) { $scope += "/$($match.Groups[1].Value)=$($match.Groups[2].Value)" }
			if ($sample.Dispatched -ne 1 -or $sample.Deadflag -ne 0 -or $sample.Ready -ne 1) {
				$windows.Remove($actor)
				continue
			}
			++$readyCount
			if (-not $windows.ContainsKey($actor) -or $windows[$actor].Scope -ne $scope -or
				($windows[$actor].Samples.Count -gt 0 -and $sample.Frame -le $windows[$actor].Samples[-1].Frame)) {
				$windows[$actor] = @{ Scope = $scope; Samples = [System.Collections.Generic.List[object]]::new() }
			}
			$window = $windows[$actor].Samples
			$window.Add($sample)
			if ($window.Count -gt 8) { $window.RemoveAt(0) }
			if ($window.Count -lt 8 -or $moving.ContainsKey($actor)) { continue }
			$progress = 0; $distance = 0.0; $valid = $true
			for ($index = 1; $index -lt 8; ++$index) {
				$dx = $window[$index].AfterX - $window[$index - 1].AfterX
				$dy = $window[$index].AfterY - $window[$index - 1].AfterY
				$delta = [math]::Sqrt($dx * $dx + $dy * $dy)
				if ($delta -gt 128.0) { $valid = $false; break }
				$distance += $delta
				if ($delta -ge 0.01) { ++$progress }
			}
			$dx = $window[7].AfterX - $window[0].AfterX
			$dy = $window[7].AfterY - $window[0].AfterY
			if ($valid -and $progress -ge 4 -and $distance -ge 8.0 -and
				[math]::Sqrt($dx * $dx + $dy * $dy) -ge 8.0) {
				$moving[$actor] = $true
				if ($movementEvidence.Count -lt 5) { $movementEvidence.Add($line) }
			}
			continue
		}
		if (-not $line.Contains('<BOT>')) { continue }
		if ($line.Contains(' attacked ') -and $line.Contains('damage') -and $attackPattern.IsMatch($line)) {
			++$attackCount
			if ($attackEvidence.Count -lt 5) { $attackEvidence.Add($line) }
		}
		if ($line -match '(?i)(Planted_The_Bomb|Bomb_Planted)') {
			++$plantCount
			if ($plantEvidence.Count -lt 3) { $plantEvidence.Add($line) }
		}
		if ($line -match '(?i)(Defused_The_Bomb|Bomb_Defused)') {
			++$defuseCount
			if ($defuseEvidence.Count -lt 3) { $defuseEvidence.Add($line) }
		}
	}
	if ($SkipLines -lt 0 -or $SkipLines -gt $totalLines) { throw "FromLine is outside the log: $SkipLines / $totalLines" }
	$objectiveGate = [pscustomobject]@{
		Passed = ($plantCount -gt 0 -and $defuseCount -gt 0)
		PlantLines = $plantCount; DefuseLines = $defuseCount
		Evidence = @($plantEvidence.ToArray()) + @($defuseEvidence.ToArray())
	}
	[pscustomobject]@{
		Movement = [pscustomobject]@{
			Passed = ($readyCount -gt 0 -and $moving.Count -gt 0 -and $idleCount -eq 0)
			ReadySamples = $readyCount; MovingSamples = $moving.Count; IdleKickLines = $idleCount
			Evidence = @($movementEvidence.ToArray())
		}
		Combat = [pscustomobject]@{
			Passed = ($attackCount -gt 0); BotAttackLines = $attackCount
			Evidence = @($attackEvidence.ToArray())
		}
		C4 = $objectiveGate; Objective = $objectiveGate; LogLines = $logLines
	}
}

if ($SelfTest) {
	$sample = @(
		'movement physics actor=1 generation=1 frame=10 sequence=1 dispatched=1 before=(0.0 0.0 0.0) after=(32.0 0.0 0.0) velocity=(100.0 0.0 0.0) grounded=1 ducked=0 ladder=0 groundentity=1 flags=1 deadflag=0 team=0 teamConfirmed=1 solid=3 movetype=3 health=100.0 ready=1 result=0',
		'movement physics actor=1 generation=1 frame=11 sequence=2 dispatched=1 before=(32.0 0.0 0.0) after=(64.0 0.0 0.0) velocity=(100.0 0.0 0.0) grounded=1 ducked=0 ladder=0 groundentity=1 flags=1 deadflag=0 team=0 teamConfirmed=1 solid=3 movetype=3 health=100.0 ready=1 result=0',
		'movement physics actor=1 generation=1 frame=12 sequence=3 dispatched=1 before=(64.0 0.0 0.0) after=(96.0 0.0 0.0) velocity=(100.0 0.0 0.0) grounded=1 ducked=0 ladder=0 groundentity=1 flags=1 deadflag=0 team=0 teamConfirmed=1 solid=3 movetype=3 health=100.0 ready=1 result=0',
		'movement physics actor=1 generation=1 frame=13 sequence=4 dispatched=1 before=(96.0 0.0 0.0) after=(128.0 0.0 0.0) velocity=(100.0 0.0 0.0) grounded=1 ducked=0 ladder=0 groundentity=1 flags=1 deadflag=0 team=0 teamConfirmed=1 solid=3 movetype=3 health=100.0 ready=1 result=0',
		'movement physics actor=1 generation=1 frame=14 sequence=5 dispatched=1 before=(128.0 0.0 0.0) after=(160.0 0.0 0.0) velocity=(100.0 0.0 0.0) grounded=1 ducked=0 ladder=0 groundentity=1 flags=1 deadflag=0 team=0 teamConfirmed=1 solid=3 movetype=3 health=100.0 ready=1 result=0',
		'movement physics actor=1 generation=1 frame=15 sequence=6 dispatched=1 before=(160.0 0.0 0.0) after=(192.0 0.0 0.0) velocity=(100.0 0.0 0.0) grounded=1 ducked=0 ladder=0 groundentity=1 flags=1 deadflag=0 team=0 teamConfirmed=1 solid=3 movetype=3 health=100.0 ready=1 result=0',
		'movement physics actor=1 generation=1 frame=16 sequence=7 dispatched=1 before=(192.0 0.0 0.0) after=(224.0 0.0 0.0) velocity=(100.0 0.0 0.0) grounded=1 ducked=0 ladder=0 groundentity=1 flags=1 deadflag=0 team=0 teamConfirmed=1 solid=3 movetype=3 health=100.0 ready=1 result=0',
		'movement physics actor=1 generation=1 frame=17 sequence=8 dispatched=1 before=(224.0 0.0 0.0) after=(256.0 0.0 0.0) velocity=(100.0 0.0 0.0) grounded=1 ducked=0 ladder=0 groundentity=1 flags=1 deadflag=0 team=0 teamConfirmed=1 solid=3 movetype=3 health=100.0 ready=1 result=0',
		'08:00: "BotA<1><BOT><TERRORIST>" attacked "BotB<2><BOT><CT>" "glock18" (damage "20") (health "80")',
		'08:10: "BotA<1><BOT><TERRORIST>" triggered "Planted_The_Bomb"',
		'08:20: "BotB<2><BOT><CT>" triggered "Defused_The_Bomb"'
	)
	$result = Get-LiveLogResult $sample
	Assert-Condition $result.Movement.Passed 'movement'
	Assert-Condition $result.Combat.Passed 'combat'
	Assert-Condition $result.C4.Passed 'C4'
	Assert-Condition $result.Objective.Passed 'objective'
	$objectiveMissingDefuse = Get-LiveLogResult @(
		'10:00: "PlantBot<1><BOT><TERRORIST>" triggered "Planted_The_Bomb"'
	)
	Assert-Condition (-not $objectiveMissingDefuse.Objective.Passed) 'objective requires both plant and defuse'
	$mixedGeneration = @($sample[0..7])
	for ($index = 4; $index -lt 8; ++$index) {
		$mixedGeneration[$index] = $mixedGeneration[$index].Replace('generation=1', 'generation=2')
	}
	Assert-Condition (-not (Get-LiveLogResult $mixedGeneration).Movement.Passed) 'movement cannot span slot reuse'
	$mixedRound = @($sample[0..7])
	for ($index = 0; $index -lt 8; ++$index) {
		$round = if ($index -lt 4) { 1 } else { 2 }
		$mixedRound[$index] += " map=1 round=$round"
	}
	Assert-Condition (-not (Get-LiveLogResult $mixedRound).Movement.Passed) 'movement cannot span round changes'
	$legacyRound = @($sample[0..3]) + @('World triggered "Round_Start"') + @($sample[4..7])
	Assert-Condition (-not (Get-LiveLogResult $legacyRound).Movement.Passed) 'legacy logs respect round boundary events'
	$backwards = @($sample[7..0])
	Assert-Condition (-not (Get-LiveLogResult $backwards).Movement.Passed) 'out-of-order frames cannot establish movement'
	Assert-Condition (-not (Get-LiveLogResult -Lines $sample -SkipLines 4).Movement.Passed) 'offset excludes earlier movement evidence'
	Assert-Condition (-not (Get-LiveLogResult @('profile window game_seconds=1 wall_seconds=1 frames=100')).Movement.Passed) 'summary alone cannot prove physical movement'
	if ($Json) {
		$result | ConvertTo-Json -Depth 6
	}
	else {
		'phase8 live log verifier self-test: PASS'
	}
	exit 0
}

if ([string]::IsNullOrWhiteSpace($LogPath)) {
	throw 'LogPath is required unless -SelfTest is used.'
}
if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
	throw "log file not found: $LogPath"
}
$resolvedLogPath = (Resolve-Path -LiteralPath $LogPath).ProviderPath
$result = Get-LiveLogResult -Lines ([System.IO.File]::ReadLines($resolvedLogPath, [System.Text.Encoding]::Default)) -SkipLines $FromLine

if ($Json) {
	$result | ConvertTo-Json -Depth 6
}
else {
	Write-Output "Phase 8 live log: lines=$($result.LogLines) fromLine=$FromLine"
	Write-Output "Movement: passed=$($result.Movement.Passed) readySamples=$($result.Movement.ReadySamples) movingSamples=$($result.Movement.MovingSamples) idleKickLines=$($result.Movement.IdleKickLines)"
	Write-Output "Combat: passed=$($result.Combat.Passed) botAttackLines=$($result.Combat.BotAttackLines)"
	Write-Output "C4: passed=$($result.C4.Passed) plantLines=$($result.C4.PlantLines) defuseLines=$($result.C4.DefuseLines)"
	Write-Output "Objective: passed=$($result.Objective.Passed) plantLines=$($result.Objective.PlantLines) defuseLines=$($result.Objective.DefuseLines)"
}

$failed = @($Require | Where-Object { -not $result.$_.Passed })
if ($failed.Count -gt 0) {
	Write-Error ("Required live log checks failed: " + ($failed -join ', '))
	exit 2
}
exit 0
