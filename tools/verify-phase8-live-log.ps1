[CmdletBinding()]
param(
	[Parameter(Mandatory = $false)]
	[string]$LogPath,
	[Parameter(Mandatory = $false)]
	[int]$FromLine = 0,
	[ValidateRange(0, 32)]
	[int]$ExpectedBots = 0,
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
	$timeMatch = [regex]::Match($Line, '(?:^| )now=(\d+(?:\.\d+)?)(?: |$)')
	[pscustomobject]@{
		Actor = [int]$match.Groups['actor'].Value
		Generation = [int]$match.Groups['generation'].Value
		Frame = [int]$match.Groups['frame'].Value
		Dispatched = [int]$match.Groups['dispatched'].Value
		Deadflag = [int]$match.Groups['deadflag'].Value
		Team = [int]$match.Groups['team'].Value
		TeamConfirmed = if ($match.Groups['teamConfirmed'].Success) { [int]$match.Groups['teamConfirmed'].Value } else { 0 }
		Ready = [int]$match.Groups['ready'].Value
		ObservedAt = if ($timeMatch.Success) { [double]$timeMatch.Groups[1].Value } else { $null }
		BeforeX = [double]$match.Groups['bx'].Value
		BeforeY = [double]$match.Groups['by'].Value
		AfterX = [double]$match.Groups['ax'].Value
		AfterY = [double]$match.Groups['ay'].Value
		Line = $Line
	}
}

function New-LiveLogState {
	@{
		Windows = @{}; Moving = @{}; Observed = @{}; LastFrame = 0; LastTime = $null
		MovementEvidence = @{}; Ready = 0; Idle = 0; Attacks = 0; Plants = 0; Defuses = 0
		TelemetryEnabled = $null; CaptureBoundaryKnown = $false; CaptureStartFrame = 0; CaptureStartTime = $null
		AttackEvidence = [System.Collections.Generic.List[string]]::new()
		PlantEvidence = [System.Collections.Generic.List[string]]::new()
		DefuseEvidence = [System.Collections.Generic.List[string]]::new()
	}
}

function Get-LiveLogResult {
	param([System.Collections.IEnumerable]$Lines, [int]$SkipLines = 0, [int]$RequiredBots = 0)
	if ($SkipLines -lt 0) { throw 'FromLine must be nonnegative.' }
	$state = New-LiveLogState
	$prefix = '(?:L \d{2}/\d{2}/\d{4} - \d{2}:\d{2}:\d{2}: |\d{2}:\d{2}(?::\d{2})?: )?'
	$bot = '"[^"\r\n]*<\d+><BOT><[^"\r\n>]*>"'
	$attackPattern = [regex]::new('^' + $prefix + $bot + '\s+attacked\s+' + $bot + '.*?\(damage "(?<damage>\d+(?:\.\d+)?)"\)', 'Compiled')
	$eventPattern = [regex]::new('^' + $prefix + $bot + '\s+triggered "(?<event>Planted_The_Bomb|Bomb_Planted|Defused_The_Bomb|Bomb_Defused)"(?:\s+.*)?$', 'Compiled')
	$mapPattern = [regex]::new('^' + $prefix + 'Loading map "[^"\r\n]+"', 'Compiled')
	$roundPattern = [regex]::new('^' + $prefix + 'World triggered "Round_Start"', 'Compiled')
	$modePattern = [regex]::new('^' + $prefix + 'Server cvar "astrabot_profile" = "(?<value>[^"]*)"', 'Compiled')
	$capturePattern = [regex]::new('^profile capture enabled=(?<enabled>[01])(?: |$)', 'Compiled')
	$scopePattern = [regex]::new('(?:^| )(map|round)=(\d+)', 'Compiled')
	$totalLines = 0; $logLines = 0; $scopeStartLine = $SkipLines + 1
	foreach ($line in $Lines) {
		++$totalLines
		if ($totalLines -le $SkipLines) { continue }
		++$logLines
		if ($line.StartsWith('Protocol version ') -or $mapPattern.IsMatch($line)) {
			$state = New-LiveLogState
			$scopeStartLine = $totalLines
		}
		if ($roundPattern.IsMatch($line)) {
			$state.Windows.Clear(); $state.Moving.Clear(); $state.MovementEvidence.Clear()
		}
		$mode = $modePattern.Match($line)
		if ($mode.Success -and -not $state.CaptureBoundaryKnown) {
			$modeValue = 0.0
			[void][double]::TryParse($mode.Groups['value'].Value, [System.Globalization.NumberStyles]::Float,
				[System.Globalization.CultureInfo]::InvariantCulture, [ref]$modeValue)
			$state.TelemetryEnabled = $modeValue -gt 0.0
			$state.Windows.Clear(); $state.Moving.Clear(); $state.MovementEvidence.Clear()
		}
		$capture = $capturePattern.Match($line)
		if ($capture.Success) {
			$state.CaptureBoundaryKnown = $true
			$state.TelemetryEnabled = $capture.Groups['enabled'].Value -eq '1'
			$frameMatch = [regex]::Match($line, '(?:^| )frame=(\d+)(?: |$)')
			$timeMatch = [regex]::Match($line, '(?:^| )now=(\d+(?:\.\d+)?)(?: |$)')
			$state.CaptureStartFrame = if ($frameMatch.Success) { [long]$frameMatch.Groups[1].Value } else { 0 }
			$state.CaptureStartTime = if ($timeMatch.Success) { [double]$timeMatch.Groups[1].Value } else { $null }
			$state.Windows.Clear(); $state.Moving.Clear(); $state.MovementEvidence.Clear()
		}
		if ($line.Contains('Game_idle_kick')) { ++$state.Idle }
		if ($line.StartsWith('movement physics ')) {
			if ($state.TelemetryEnabled -eq $false) {
				if ($state.CaptureBoundaryKnown) { continue }
				# Old producers can emit physical samples after the last logged CVar value.
				$state.TelemetryEnabled = $null
			}
			$sample = Parse-MovementSample $line
			if ($null -eq $sample -or $sample.Actor -lt 1 -or $sample.Actor -gt 32) { continue }
			if ($state.CaptureBoundaryKnown -and ($sample.Frame -lt $state.CaptureStartFrame -or
				($null -ne $sample.ObservedAt -and $null -ne $state.CaptureStartTime -and $sample.ObservedAt -lt $state.CaptureStartTime))) { continue }
			$actor = $sample.Actor
			$scopeFields = @{map = '0'; round = '0'}
			foreach ($match in $scopePattern.Matches($line)) { $scopeFields[$match.Groups[1].Value] = $match.Groups[2].Value }
			$scope = "$($sample.Generation)/$($scopeFields.map)/$($scopeFields.round)"
			if (-not $state.Observed.ContainsKey($actor) -or $state.Observed[$actor] -ne $scope) {
				$state.Windows.Remove($actor); $state.Moving.Remove($actor); $state.MovementEvidence.Remove($actor)
			}
			$state.Observed[$actor] = $scope
			$state.LastFrame = [math]::Max($state.LastFrame, $sample.Frame)
			if ($null -ne $sample.ObservedAt) {
				$state.LastTime = if ($null -eq $state.LastTime) { $sample.ObservedAt } else { [math]::Max($state.LastTime, $sample.ObservedAt) }
			}
			if ($sample.Dispatched -ne 1 -or $sample.Deadflag -ne 0 -or $sample.Ready -ne 1) {
				$state.Windows.Remove($actor); $state.Moving.Remove($actor); $state.MovementEvidence.Remove($actor)
				continue
			}
			++$state.Ready
			$resetWindow = -not $state.Windows.ContainsKey($actor)
			if (-not $resetWindow -and $state.Windows[$actor].Count -gt 0) {
				$previous = $state.Windows[$actor][-1]
				$timed = $null -ne $sample.ObservedAt -and $null -ne $previous.ObservedAt
				$gapTooLarge = if ($timed) { $sample.ObservedAt - $previous.ObservedAt -gt 2.0 } else { $sample.Frame - $previous.Frame -gt 128 }
				$resetWindow = $sample.Frame -le $previous.Frame -or $gapTooLarge -or
					($timed -and $sample.ObservedAt -le $previous.ObservedAt)
			}
			if ($resetWindow) {
				$state.Windows[$actor] = [System.Collections.Generic.List[object]]::new()
				$state.Moving.Remove($actor); $state.MovementEvidence.Remove($actor)
			}
			$window = $state.Windows[$actor]
			$window.Add($sample)
			if ($window.Count -gt 8) { $window.RemoveAt(0) }
			if ($window.Count -lt 8) { continue }
			$progress = 0; $distance = 0.0; $valid = $true; $lastMove = 0; $lastMoveTime = $null
			for ($index = 0; $index -lt 8; ++$index) {
				$dx = $window[$index].AfterX - $window[$index].BeforeX
				$dy = $window[$index].AfterY - $window[$index].BeforeY
				$delta = [math]::Sqrt($dx * $dx + $dy * $dy)
				if ($delta -gt 128.0) { $valid = $false; break }
				$distance += $delta
				if ($delta -ge 0.01) { ++$progress; $lastMove = $window[$index].Frame; $lastMoveTime = $window[$index].ObservedAt }
				if ($index -gt 0) {
					$dx = $window[$index].AfterX - $window[$index - 1].AfterX
					$dy = $window[$index].AfterY - $window[$index - 1].AfterY
					if ([math]::Sqrt($dx * $dx + $dy * $dy) -gt 128.0) { $valid = $false; break }
				}
			}
			$dx = $window[7].AfterX - $window[0].AfterX
			$dy = $window[7].AfterY - $window[0].AfterY
			if ($valid -and $progress -ge 4 -and $distance -ge 8.0 -and
				[math]::Sqrt($dx * $dx + $dy * $dy) -ge 8.0) {
				$state.Moving[$actor] = @{ Frame = $lastMove; Time = $lastMoveTime }
				$state.MovementEvidence[$actor] = $line
			}
			continue
		}
		if (-not $line.Contains('<BOT>')) { continue }
		$attack = $attackPattern.Match($line)
		if ($attack.Success -and [double]$attack.Groups['damage'].Value -gt 0.0) {
			++$state.Attacks
			if ($state.AttackEvidence.Count -lt 5) { $state.AttackEvidence.Add($line) }
		}
		$event = $eventPattern.Match($line)
		if ($event.Success) {
			if ($event.Groups['event'].Value -in @('Planted_The_Bomb', 'Bomb_Planted')) {
				++$state.Plants
				if ($state.PlantEvidence.Count -lt 3) { $state.PlantEvidence.Add($line) }
			} else {
				++$state.Defuses
				if ($state.DefuseEvidence.Count -lt 3) { $state.DefuseEvidence.Add($line) }
			}
		}
	}
	if ($SkipLines -gt $totalLines) { throw "FromLine is outside the log: $SkipLines / $totalLines" }
	$observed = @($state.Observed.Keys | Sort-Object)
	$verified = @($observed | Where-Object {
		if (-not $state.Moving.ContainsKey($_)) { return $false }
		$motion = $state.Moving[$_]
		if ($null -ne $motion.Time -and $null -ne $state.LastTime) { return $state.LastTime - $motion.Time -le 2.0 }
		return $state.LastFrame - $motion.Frame -le 128
	})
	$unverified = @($observed | Where-Object { $_ -notin $verified })
	$expected = if ($RequiredBots -gt 0) { $RequiredBots } else { $observed.Count }
	$movementPassed = $expected -gt 0 -and $observed.Count -eq $expected -and $verified.Count -eq $expected -and $state.Idle -eq 0
	$movementEvidence = @($verified | Select-Object -First 5 | ForEach-Object { $state.MovementEvidence[$_] })
	$objectiveGate = [pscustomobject]@{
		Passed = ($state.Plants -gt 0 -and $state.Defuses -gt 0)
		PlantLines = $state.Plants; DefuseLines = $state.Defuses
		Evidence = @($state.PlantEvidence.ToArray()) + @($state.DefuseEvidence.ToArray())
	}
	[pscustomobject]@{
		Scope = 'latest session/map after FromLine'; ScopeStartLine = $scopeStartLine
		Movement = [pscustomobject]@{
			Passed = $movementPassed
			Status = if ($movementPassed) { 'Verified' } elseif ($state.Idle -gt 0) { 'IdleKickObserved' } elseif ($state.TelemetryEnabled -eq $false) { 'TelemetryDisabled' } else { 'InsufficientEvidence' }
			ReadySamples = $state.Ready; MovingSamples = $verified.Count; IdleKickLines = $state.Idle
			ExpectedBots = $expected; ObservedActors = $observed; VerifiedActors = $verified; UnverifiedActors = $unverified
			MissingActorCount = [math]::Max(0, $expected - $observed.Count)
			Coverage = 'all observed/expected actors; current generation/round; motion within 2s of latest telemetry (legacy: 128 frames)'
			Evidence = $movementEvidence
		}
		Combat = [pscustomobject]@{
			Passed = ($state.Attacks -gt 0); BotAttackLines = $state.Attacks
			Evidence = @($state.AttackEvidence.ToArray())
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
	$namesOnly = @(
		'10:00: "Planted_The_Bomb<1><BOT><TERRORIST>" purchased "ak47"',
		'10:01: "Defused_The_Bomb<2><BOT><CT>" purchased "m4a1"'
	)
	Assert-Condition (-not (Get-LiveLogResult $namesOnly).Objective.Passed) 'names containing event text are not objective events'
	$zeroDisplacement = @($sample[0..7])
	for ($index = 0; $index -lt 8; ++$index) {
		$position = 32 * ($index + 1)
		$zeroDisplacement[$index] = $zeroDisplacement[$index] -replace 'before=\([^)]*\)', "before=($position.0 0.0 0.0)"
	}
	Assert-Condition (-not (Get-LiveLogResult $zeroDisplacement).Movement.Passed) 'external relocation is not dispatched movement'
	$newSession = @($sample) + @('Protocol version 48', 'Loading map "de_dust2"')
	Assert-Condition (-not (Get-LiveLogResult $newSession).Movement.Passed) 'old session movement cannot verify a new session'
	Assert-Condition (-not (Get-LiveLogResult $newSession).Objective.Passed) 'old session objective cannot verify a new session'
	$disabled = @($sample) + @('L 11/12/2026 - 10:00:00: Server cvar "astrabot_profile" = "0"')
	Assert-Condition (-not (Get-LiveLogResult $disabled).Movement.Passed) 'disabled telemetry cannot retain an old movement pass'
	$legacyMode = @('L 11/12/2026 - 10:00:00: Server cvar "astrabot_profile" = "0"') + @($sample)
	Assert-Condition (Get-LiveLogResult $legacyMode).Movement.Passed 'legacy samples are not discarded by stale CVar metadata'
	$producerStopped = @($sample) + @('profile capture enabled=0 map=1 round=1 frame=20 now=1.0')
	Assert-Condition (-not (Get-LiveLogResult $producerStopped).Movement.Passed) 'producer stop clears prior movement evidence'
	$restartedRecords = @($sample[0..7])
	for ($index = 0; $index -lt 8; ++$index) { $restartedRecords[$index] = $restartedRecords[$index] -replace 'frame=\d+', ('frame=' + (40 + $index)) }
	$producerRestarted = @($producerStopped) + @('profile capture enabled=1 map=1 round=1 frame=30 now=2.0') + $restartedRecords
	Assert-Condition (Get-LiveLogResult $producerRestarted).Movement.Passed 'producer restart establishes a new capture interval'
	$replayed = @($producerStopped) + @('profile capture enabled=1 map=1 round=1 frame=30 now=2.0') + @($sample)
	Assert-Condition (-not (Get-LiveLogResult $replayed).Movement.Passed) 'records before a producer capture boundary cannot be replayed as fresh evidence'
	Assert-Condition (-not (Get-LiveLogResult -Lines $sample -RequiredBots 2).Movement.Passed) 'one actor cannot verify two expected bots'
	$twoActors = @($sample[0..7]) + @($sample[0..7] | ForEach-Object { $_.Replace('actor=1 ', 'actor=2 ') })
	Assert-Condition (Get-LiveLogResult -Lines $twoActors -RequiredBots 2).Movement.Passed 'all expected actors can verify movement'
	$oldGeneration = @($sample[0..7]) + @($sample[0].Replace('generation=1', 'generation=2').Replace('frame=10', 'frame=100'))
	Assert-Condition (-not (Get-LiveLogResult $oldGeneration).Movement.Passed) 'a reused actor must establish its own movement'
	$stopped = @($sample[0..7]) + @($sample[7].Replace('frame=17', 'frame=300').Replace('after=(256.0', 'after=(224.0'))
	Assert-Condition (-not (Get-LiveLogResult $stopped).Movement.Passed) 'old motion cannot verify current stationary telemetry'
	$fastFrames = @($sample[0..7])
	for ($index = 0; $index -lt 8; ++$index) {
		$fastFrames[$index] = ($fastFrames[$index] -replace 'frame=\d+', ('frame=' + (1000 * ($index + 1)))) +
			(' now=' + ($index * 0.05).ToString('F3', [System.Globalization.CultureInfo]::InvariantCulture))
	}
	Assert-Condition (Get-LiveLogResult $fastFrames).Movement.Passed 'timestamped motion is independent of server FPS'
	$staleTime = @($fastFrames) + @($fastFrames[7].Replace('frame=8000', 'frame=8001').Replace('now=0.350', 'now=20.000').Replace('after=(256.0', 'after=(224.0'))
	Assert-Condition (-not (Get-LiveLogResult $staleTime).Movement.Passed) 'timestamps reject stale motion even with adjacent frame numbers'
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
$result = Get-LiveLogResult -Lines ([System.IO.File]::ReadLines($resolvedLogPath, [System.Text.Encoding]::Default)) -SkipLines $FromLine -RequiredBots $ExpectedBots

if ($Json) {
	$result | ConvertTo-Json -Depth 6
}
else {
	Write-Output "Phase 8 live log: lines=$($result.LogLines) fromLine=$FromLine"
	Write-Output "Scope: $($result.Scope) startLine=$($result.ScopeStartLine)"
	Write-Output "Movement coverage: expected=$($result.Movement.ExpectedBots) observed=$($result.Movement.ObservedActors.Count) missing=$($result.Movement.MissingActorCount) unverified=$($result.Movement.UnverifiedActors -join ',') status=$($result.Movement.Status)"
	Write-Output "Movement: passed=$($result.Movement.Passed) readySamples=$($result.Movement.ReadySamples) movingSamples=$($result.Movement.MovingSamples) idleKickLines=$($result.Movement.IdleKickLines)"
	Write-Output "Combat: passed=$($result.Combat.Passed) botAttackLines=$($result.Combat.BotAttackLines)"
	Write-Output "C4: passed=$($result.C4.Passed) plantLines=$($result.C4.PlantLines) defuseLines=$($result.C4.DefuseLines)"
	Write-Output "Objective: passed=$($result.Objective.Passed) plantLines=$($result.Objective.PlantLines) defuseLines=$($result.Objective.DefuseLines)"
}

$failed = @($Require | Where-Object { -not $result.$_.Passed })
if ($failed.Count -gt 0) {
	Write-Error ("Required live log checks failed: " + ($failed -join ', ')) -ErrorAction Continue
	exit 2
}
exit 0
