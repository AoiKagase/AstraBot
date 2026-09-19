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
	$dx = [double]$match.Groups['ax'].Value - [double]$match.Groups['bx'].Value
	$dy = [double]$match.Groups['ay'].Value - [double]$match.Groups['by'].Value
	[pscustomobject]@{
		Actor = [int]$match.Groups['actor'].Value
		Generation = [int]$match.Groups['generation'].Value
		Frame = [int]$match.Groups['frame'].Value
		Dispatched = [int]$match.Groups['dispatched'].Value
		Deadflag = [int]$match.Groups['deadflag'].Value
		Team = [int]$match.Groups['team'].Value
		TeamConfirmed = if ($match.Groups['teamConfirmed'].Success) { [int]$match.Groups['teamConfirmed'].Value } else { 0 }
		Ready = [int]$match.Groups['ready'].Value
		HorizontalDelta = [math]::Sqrt(($dx * $dx) + ($dy * $dy))
		AfterX = [double]$match.Groups['ax'].Value
		AfterY = [double]$match.Groups['ay'].Value
		Line = $Line
	}
}

function Get-LiveLogResult {
	param([string[]]$Lines)
	$samples = @($Lines | ForEach-Object { Parse-MovementSample $_ } | Where-Object { $null -ne $_ })
	$readySamples = @($samples | Where-Object { $_.Dispatched -eq 1 -and $_.Deadflag -eq 0 -and $_.Ready -eq 1 })
	$movingSamples = @()
	$movementWindowSamples = 8
	$minimumNetProgress = 8.0
	$maximumSampleJump = 128.0
	foreach ($actorSamples in @($readySamples | Group-Object Actor)) {
		$ordered = @($actorSamples.Group | Sort-Object Frame)
		for ($start = 0; $start -le $ordered.Count - $movementWindowSamples; ++$start) {
			$first = $ordered[$start]
			$last = $ordered[$start + $movementWindowSamples - 1]
			$progressSamples = 0
			$pathDistance = 0.0
			$validWindow = $true
			for ($offset = 1; $offset -lt $movementWindowSamples; ++$offset) {
				$previous = $ordered[$start + $offset - 1]
				$sample = $ordered[$start + $offset]
				$dx = $sample.AfterX - $previous.AfterX
				$dy = $sample.AfterY - $previous.AfterY
				$delta = [math]::Sqrt(($dx * $dx) + ($dy * $dy))
				if ($delta -gt $maximumSampleJump) {
					$validWindow = $false
					break
				}
				$pathDistance += $delta
				if ($delta -ge 0.01) {
					++$progressSamples
				}
			}
			$netDx = $last.AfterX - $first.AfterX
			$netDy = $last.AfterY - $first.AfterY
			$netProgress = [math]::Sqrt(($netDx * $netDx) + ($netDy * $netDy))
			if ($validWindow -and $progressSamples -ge 4 -and
				$pathDistance -ge $minimumNetProgress -and
				$netProgress -ge $minimumNetProgress) {
				$movingSamples += $last
				break
			}
		}
	}
	$idleKickLines = @($Lines | Where-Object { $_ -match 'Game_idle_kick' })
	$botAttackLines = @($Lines | Where-Object {
		$_ -match '"[^"\r\n]*<BOT>[^"\r\n]*"\s+attacked\s+"[^"\r\n]*<BOT>[^"\r\n]*"' -and
		$_ -match 'damage'
	})
	$plantLines = @($Lines | Where-Object {
		$_ -match '(?i)(Planted_The_Bomb|Bomb_Planted)' -and $_ -match '<BOT>'
	})
	$defuseLines = @($Lines | Where-Object {
		$_ -match '(?i)(Defused_The_Bomb|Bomb_Defused)' -and $_ -match '<BOT>'
	})
	$objectiveGate = [pscustomobject]@{
		Passed = ($plantLines.Count -gt 0 -and $defuseLines.Count -gt 0)
		PlantLines = $plantLines.Count
		DefuseLines = $defuseLines.Count
		Evidence = @($plantLines | Select-Object -First 3) + @($defuseLines | Select-Object -First 3)
	}
	[pscustomobject]@{
		Movement = [pscustomobject]@{
			Passed = ($readySamples.Count -gt 0 -and $movingSamples.Count -gt 0 -and $idleKickLines.Count -eq 0)
			ReadySamples = $readySamples.Count
			MovingSamples = $movingSamples.Count
			IdleKickLines = $idleKickLines.Count
			Evidence = @($movingSamples | Select-Object -First 5 | ForEach-Object { $_.Line })
		}
		Combat = [pscustomobject]@{
			Passed = ($botAttackLines.Count -gt 0)
			BotAttackLines = $botAttackLines.Count
			Evidence = @($botAttackLines | Select-Object -First 5)
		}
		# Keep C4 as the historical name while exposing the objective gate
		# explicitly for phase acceptance and independent -Require selection.
		C4 = $objectiveGate
		Objective = $objectiveGate
		LogLines = $Lines.Count
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
$allLines = @(Get-Content -LiteralPath $LogPath)
if ($FromLine -lt 0 -or $FromLine -gt $allLines.Count) {
	throw "FromLine is outside the log: $FromLine / $($allLines.Count)"
}
$lines = @($allLines | Select-Object -Skip $FromLine)
$result = Get-LiveLogResult $lines

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
