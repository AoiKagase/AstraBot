# SPDX-License-Identifier: MPL-2.0
# Opt-in independent v5 byte/geometry/Dijkstra comparison. Does not start a server.
param(
    [Parameter(Mandatory=$true)][string]$NavPath,
    [Parameter(Mandatory=$true)][string]$BspPath,
    [Parameter(Mandatory=$true)][string]$Inspector,
    [Parameter(Mandatory=$true)][uint32]$GoalArea,
    [uint32]$StartArea = 1,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$culture = [Globalization.CultureInfo]::InvariantCulture
function Hash([string]$path) {
    $stream = [IO.File]::OpenRead($path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
    finally { $stream.Dispose(); $sha.Dispose() }
}
function Require([bool]$condition, [string]$reason) { if (-not $condition) { throw $reason } }
function Close([double]$a, [double]$b) { return [Math]::Abs($a-$b) -le (1e-8 * [Math]::Max(1, [Math]::Abs($b))) }
function Distance($a, $b) {
    $sum = 0.0
    for ($i=0; $i -lt 3; ++$i) { $delta=$a[$i]-$b[$i]; $sum += $delta*$delta }
    return [Math]::Sqrt($sum)
}
function Real {
    $v = $r.ReadSingle()
    Require (-not [single]::IsNaN($v) -and -not [single]::IsInfinity($v)) 'nonfinite geometry'
    return [double]$v
}
function Count {
    $n = $r.ReadUInt32(); Require ($n -le 1000000) 'independent count cap'
    return $n
}
function Point($area) {
    # Query coordinates cross the CLI's float boundary explicitly.
    return @($area.Center | ForEach-Object { [double][single]$_ })
}
function Match($point, [bool]$containingOnly) {
    $best = $null; $distance = [double]::PositiveInfinity
    foreach ($a in $areas.Values) {
        $g=$a.Geometry
        $x=[Math]::Max($g[0],[Math]::Min($g[3],$point[0]))
        $y=[Math]::Max($g[1],[Math]::Min($g[4],$point[1]))
        if ($containingOnly -and ($x -ne $point[0] -or $y -ne $point[1])) { continue }
        $u=($x-$g[0])/($g[3]-$g[0]); $v=($y-$g[1])/($g[4]-$g[1])
        $z=(1-$v)*((1-$u)*$g[2]+$u*$g[6])+$v*((1-$u)*$g[7]+$u*$g[5])
        $dz=$z-$point[2]; $dx=$x-$point[0]; $dy=$y-$point[1]
        if ([Math]::Abs($dz) -gt 1 -or ($dx*$dx+$dy*$dy) -gt 1) { continue }
        $d=$dx*$dx+$dy*$dy+$dz*$dz
        if ($d -lt $distance -or ($d -eq $distance -and $a.Id -lt $best.Id)) {
            $distance=$d; $best=[pscustomobject]@{Id=$a.Id;DistanceSquared=$d;Projected=@($x,$y,$z)}
        }
    }
    return $best
}
function Shortest([uint32]$start, [uint32]$goal) {
    $dist=@{}; $done=New-Object 'Collections.Generic.HashSet[uint32]'; $dist[$start]=0.0
    while ($true) {
        $current=$null; $best=[double]::PositiveInfinity
        foreach ($id in $dist.Keys) {
            if (-not $done.Contains($id) -and ($dist[$id] -lt $best -or
                ($dist[$id] -eq $best -and ($null -eq $current -or $id -lt $current)))) {
                $current=$id; $best=$dist[$id]
            }
        }
        if ($null -eq $current) { return [double]::PositiveInfinity }
        if ($current -eq $goal) { return $best }
        $null=$done.Add($current)
        foreach ($edge in $areas[$current].Edges) {
            $candidate=$best+(Distance $areas[$current].Center $areas[$edge.Target].Center)
            if (-not $dist.ContainsKey($edge.Target) -or $candidate -lt $dist[$edge.Target]) { $dist[$edge.Target]=$candidate }
        }
    }
}
$NavPath=(Resolve-Path -LiteralPath $NavPath).Path
$BspPath=(Resolve-Path -LiteralPath $BspPath).Path
$Inspector=(Resolve-Path -LiteralPath $Inspector).Path
$beforeNav=Hash $NavPath; $beforeBsp=Hash $BspPath
$navLength=(Get-Item -LiteralPath $NavPath).Length; $bspLength=(Get-Item -LiteralPath $BspPath).Length
Require ($navLength -le 64*1024*1024) 'NAV input cap'
$f=[IO.File]::OpenRead($NavPath); $r=New-Object IO.BinaryReader($f)
$areas=@{}; $hiding=New-Object 'Collections.Generic.HashSet[uint32]'
$approachRefs=New-Object 'Collections.Generic.List[uint32]'
$encounterAreas=New-Object 'Collections.Generic.List[uint32]'
$spotRefs=New-Object 'Collections.Generic.List[uint32]'
$connectionCount=0; $approachCount=0; $encounterCount=0; $zeroHiding=0; $zeroApproach=0
try {
    Require ($r.ReadUInt32() -eq 4277009102) 'magic FEEDFACE'
    Require ($r.ReadUInt32() -eq 5) 'independent checker supports v5 only'
    Require ($r.ReadUInt32() -eq $bspLength) 'header BSP length'
    $places=$r.ReadUInt16()
    for ($i=0; $i -lt $places; ++$i) {
        $n=$r.ReadUInt16(); Require ($n -gt 0) 'Place length'
        $raw=$r.ReadBytes($n); Require ($raw.Length -eq $n -and $raw[$n-1] -eq 0) 'Place bytes'
    }
    $areaCount=Count; Require ($areaCount -gt 0 -and $areaCount -le 100000) 'area count'
    for ($i=0; $i -lt $areaCount; ++$i) {
        $id=$r.ReadUInt32(); Require ($id -ne 0 -and -not $areas.ContainsKey($id)) 'area ID'
        $attributes=$r.ReadByte(); $g=@(); for ($j=0; $j -lt 8; ++$j) { $g += (Real) }
        Require ($g[0] -lt $g[3] -and $g[1] -lt $g[4]) 'area rectangle'
        $center=@((($g[0]+$g[3])/2), (($g[1]+$g[4])/2), (($g[2]+$g[5]+$g[6]+$g[7])/4))
        $edges=New-Object 'Collections.Generic.List[object]'
        for ($d=0; $d -lt 4; ++$d) {
            $n=Count; $seen=New-Object 'Collections.Generic.HashSet[uint32]'
            for ($j=0; $j -lt $n; ++$j) {
                $target=$r.ReadUInt32(); Require ($target -ne 0 -and $target -ne $id -and $seen.Add($target)) 'connection'
                $edges.Add([pscustomobject]@{Target=$target;Direction=$d}); ++$connectionCount
            }
        }
        $n=$r.ReadByte()
        for ($j=0; $j -lt $n; ++$j) {
            $hid=$r.ReadUInt32(); Require ($hiding.Add($hid)) 'duplicate hiding ID'
            if ($hid -eq 0) { ++$zeroHiding }
            $null=Real; $null=Real; $null=Real; $null=$r.ReadByte()
        }
        $n=$r.ReadByte(); $approachCount+=$n
        for ($j=0; $j -lt $n; ++$j) {
            $approachRefs.Add($r.ReadUInt32()); $approachRefs.Add($r.ReadUInt32()); $null=$r.ReadByte()
            $approachRefs.Add($r.ReadUInt32()); $null=$r.ReadByte()
        }
        $n=Count; $encounterCount+=$n
        for ($j=0; $j -lt $n; ++$j) {
            $encounterAreas.Add($r.ReadUInt32()); Require ($r.ReadByte() -le 3) 'encounter direction'
            $encounterAreas.Add($r.ReadUInt32()); Require ($r.ReadByte() -le 3) 'encounter direction'
            $ns=$r.ReadByte(); for ($k=0; $k -lt $ns; ++$k) { $spotRefs.Add($r.ReadUInt32()); $null=$r.ReadByte() }
        }
        Require ($r.ReadUInt16() -le $places) 'Place reference'
        $areas[$id]=[pscustomobject]@{Id=$id;Geometry=$g;Center=$center;Edges=$edges}
    }
    Require ($f.Position -eq $f.Length) 'exact EOF'
} finally { $r.Dispose(); $f.Dispose() }
foreach ($a in $areas.Values) { foreach ($e in $a.Edges) { Require ($areas.ContainsKey($e.Target)) 'missing connection' } }
foreach ($id in $approachRefs) { if ($id -eq 0) { ++$zeroApproach } else { Require ($areas.ContainsKey($id)) 'missing approach' } }
foreach ($id in $encounterAreas) { Require ($areas.ContainsKey($id)) 'missing encounter area' }
foreach ($id in $spotRefs) { Require ($hiding.Contains($id)) 'missing hiding spot' }
Require ($areas.ContainsKey($StartArea) -and $areas.ContainsKey($GoalArea)) 'query area missing'
$null=New-Item -ItemType Directory -Force -Path $OutputDirectory
$results=@()
try {
    foreach ($pair in @(@($StartArea,$GoalArea),@($GoalArea,$StartArea))) {
        $start=Point $areas[$pair[0]]; $goal=Point $areas[$pair[1]]
        $sm=Match $start $true; $gm=Match $goal $true
        Require ($null -ne $sm -and $null -ne $gm -and $sm.Id -eq $pair[0] -and $gm.Id -eq $pair[1]) 'center containment'
        $sn=Match $start $false; $gn=Match $goal $false
        Require ($sn.Id -eq $sm.Id -and $gn.Id -eq $gm.Id) 'independent nearest'
        $inspectArgs=@('--nav',$NavPath,'--bsp',$BspPath,'--start')
        $inspectArgs+=@($start | ForEach-Object {$_.ToString('R',$culture)})
        $inspectArgs+='--goal'; $inspectArgs+=@($goal | ForEach-Object {$_.ToString('R',$culture)})
        $inspectArgs+=@('--radius','1','--vertical','1')
        $report=@(& $Inspector @inspectArgs); $exit=$LASTEXITCODE
        $name=[IO.Path]::GetFileNameWithoutExtension($NavPath)+'-'+$pair[0]+'-'+$pair[1]+'.txt'
        $report | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $OutputDirectory $name)
        Require ($exit -eq 0) "inspector failed: $name exit=$exit"
        $values=@{}; foreach ($line in $report) { if ($line -match '^([^=,]+)=(.*)$') { $values[$matches[1]]=$matches[2] } }
        foreach ($entry in @{version=5;areas=$areaCount;places=$places;connections=$connectionCount;hiding_spots=$hiding.Count;
            approaches=$approachCount;encounters_retained=$encounterCount;encounter_spots_retained=$spotRefs.Count;bytes_consumed=$navLength}.GetEnumerator()) {
            Require ($values[$entry.Key] -eq [string]$entry.Value) ('metadata mismatch: '+$entry.Key)
        }
        Require ($values.bsp_size_comparison -eq 'Match') 'BSP comparison'
        foreach ($entry in @{start=$sm;goal=$gm}.GetEnumerator()) {
            $label=$entry.Key; $match=$entry.Value
            Require ($values[$label+'_match'] -eq [string]$match.Id) 'query match'
            Require (Close ([double]::Parse($values[$label+'_distance_squared'],$culture)) $match.DistanceSquared) 'query distance'
            $p=$values[$label+'_projected'].Split(',')
            for ($j=0;$j -lt 3;++$j) { Require (Close ([double]::Parse($p[$j],$culture)) $match.Projected[$j]) 'projection' }
        }
        $expected=Shortest $pair[0] $pair[1]
        if ([double]::IsPositiveInfinity($expected)) {
            Require ($values.route_status -eq 'Unreachable' -and $values.route_areas -eq '') 'unreachable'
        } else {
            Require ($values.route_status -eq 'Complete') 'complete route'
            $route=@($values.route_areas.Split(',') | ForEach-Object {[uint32]$_})
            Require ($route[0] -eq $pair[0] -and $route[-1] -eq $pair[1]) 'route endpoints'
            $edgeLines=@($report | Where-Object {$_ -like 'edge=*'}); Require ($edgeLines.Count -eq $route.Count-1) 'edge count'
            $total=0.0
            for ($i=0;$i -lt $edgeLines.Count;++$i) {
                Require ($edgeLines[$i] -match '^edge=(\d+),(\d+),(\d+),traversal=0,external=false,total=([^,]+),components=(.*)$') 'edge syntax'
                $from=[uint32]$matches[1];$to=[uint32]$matches[2];$direction=[int]$matches[3];$cost=[double]::Parse($matches[4],$culture)
                $components=$matches[5].Split(',')
                Require ($from -eq $route[$i] -and $to -eq $route[$i+1]) 'edge order'
                Require (@($areas[$from].Edges | Where-Object {$_.Target -eq $to -and $_.Direction -eq $direction}).Count -eq 1) 'selected directed edge'
                $distance=Distance $areas[$from].Center $areas[$to].Center
                Require (Close $cost $distance) 'edge cost'
                Require ($components.Count -eq 4 -and (Close ([double]::Parse($components[0],$culture)) $distance)) 'edge distance component'
                for ($c=1;$c -lt 4;++$c) { Require ([double]::Parse($components[$c],$culture) -eq 0) 'unexpected edge component' }
                $total+=$distance
            }
            Require ((Close $total $expected) -and (Close ([double]::Parse($values.route_total,$culture)) $expected)) 'Dijkstra cost'
            $components=$values.route_components.Split(',')
            Require ($components.Count -eq 4 -and (Close ([double]::Parse($components[0],$culture)) $expected)) 'route distance component'
            for ($c=1;$c -lt 4;++$c) { Require ([double]::Parse($components[$c],$culture) -eq 0) 'unexpected route component' }
        }
        $results += [pscustomobject]@{Start=$pair[0];Goal=$pair[1];StartPosition=$start;GoalPosition=$goal;
            Status=$values.route_status;Areas=$values.route_areas;Cost=$values.route_total;OracleCost=$expected;Report=$name}
    }
    # Point just outside the mesh's maximum X forces the real CLI fallback,
    # unlike center queries, which normally resolve through containing.
    $boundary=@($areas.Values | Sort-Object @{Expression={$_.Geometry[3]};Descending=$true},Id)[0]
    $g=$boundary.Geometry
    $outside=@(([double][single]($g[3]+0.5)), ([double][single](($g[1]+$g[4])/2)), ([double][single](($g[6]+$g[5])/2)))
    Require ($null -eq (Match $outside $true)) 'outside query unexpectedly contained'
    $nearest=Match $outside $false; Require ($null -ne $nearest) 'outside nearest missing'
    $inspectArgs=@('--nav',$NavPath,'--bsp',$BspPath,'--start')
    $inspectArgs+=@($outside | ForEach-Object {$_.ToString('R',$culture)})
    $inspectArgs+='--goal'; $inspectArgs+=@($outside | ForEach-Object {$_.ToString('R',$culture)})
    $inspectArgs+=@('--radius','1','--vertical','1')
    $report=@(& $Inspector @inspectArgs); Require ($LASTEXITCODE -eq 0) 'nearest inspector failed'
    $name=[IO.Path]::GetFileNameWithoutExtension($NavPath)+'-nearest.txt'
    $report | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $OutputDirectory $name)
    $values=@{}; foreach ($line in $report) { if ($line -match '^([^=,]+)=(.*)$') { $values[$matches[1]]=$matches[2] } }
    Require ($values.start_method -eq 'nearestGeometry' -and $values.start_match -eq [string]$nearest.Id) 'nearest identity'
    Require (Close ([double]::Parse($values.start_distance_squared,$culture)) $nearest.DistanceSquared) 'nearest distance'
    $projected=$values.start_projected.Split(',')
    for ($j=0;$j -lt 3;++$j) { Require (Close ([double]::Parse($projected[$j],$culture)) $nearest.Projected[$j]) 'nearest projection' }
    Require ($values.route_status -eq 'Complete' -and $values.route_areas -eq [string]$nearest.Id -and $values.route_total -eq '0') 'nearest same-area route'
    $nearestEvidence=[pscustomobject]@{Position=$outside;Area=$nearest.Id;DistanceSquared=$nearest.DistanceSquared;Report=$name}
} finally {
    Require ((Hash $NavPath) -eq $beforeNav -and (Hash $BspPath) -eq $beforeBsp) 'input hash changed'
    Require ((Get-Item -LiteralPath $NavPath).Length -eq $navLength -and (Get-Item -LiteralPath $BspPath).Length -eq $bspLength) 'input size changed'
}
[pscustomobject]@{Map=[IO.Path]::GetFileNameWithoutExtension($NavPath);NavBytes=$navLength;BspBytes=$bspLength;
    NavSha256=$beforeNav;BspSha256=$beforeBsp;Areas=$areaCount;Connections=$connectionCount;HidingSpots=$hiding.Count;
    ZeroHiding=$zeroHiding;Approaches=$approachCount;ZeroApproachReferences=$zeroApproach;Encounters=$encounterCount;
    EncounterSpots=$spotRefs.Count;EOF=$true;Unchanged=$true;Routes=$results;Nearest=$nearestEvidence} | ConvertTo-Json -Depth 5
