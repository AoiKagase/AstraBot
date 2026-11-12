[CmdletBinding()]
param(
    [string]$AdapterPath = 'src\adapter\metamod\plugin_runtime.cpp',
    [switch]$RequireLiveActions,
    [switch]$Json
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $AdapterPath -PathType Leaf)) {
    throw "adapter source not found: $AdapterPath"
}

$source = Get-Content -LiteralPath $AdapterPath -Raw
$start = $source.IndexOf('void PluginRuntime::updateManagedBotMovement()')
$end = $source.IndexOf('void PluginRuntime::resetManagedBotTiming', $start)
if ($start -lt 0 -or $end -le $start) {
    throw 'could not isolate updateManagedBotMovement adapter boundary'
}

$movementBody = $source.Substring($start, $end - $start)
$adapterDirectory = Split-Path -Parent $AdapterPath
$commandSource = Get-Content -LiteralPath (Join-Path $adapterDirectory 'bot_command_runtime.cpp') -Raw
$behaviorHeader = Get-Content -LiteralPath (Join-Path $adapterDirectory 'bot_behavior_runtime.hpp') -Raw
$behaviorSource = Get-Content -LiteralPath (Join-Path $adapterDirectory 'bot_behavior_runtime.cpp') -Raw
$actionAdapterPath = Join-Path $adapterDirectory 'action_adapter.hpp'
$actionAdapterSource = if (Test-Path -LiteralPath $actionAdapterPath) {
    Get-Content -LiteralPath $actionAdapterPath -Raw
} else {
    ''
}
$result = [ordered]@{
    MovementDispatch = $movementBody.Contains('buildAndExecuteMovement') -and $commandSource.Contains('service_inputDispatcher().dispatchNext')
    CombatCoreCall = $behaviorHeader.Contains('combat::CombatController') -and $behaviorSource.Contains('managedBotCombat')
    ObjectiveCoreCall = $behaviorHeader.Contains('objectives::RoundObjectivePlanner') -and $behaviorSource.Contains('managedBotObjectives')
    AttackButton = $actionAdapterSource.Contains('IN_ATTACK')
    UseButton = $actionAdapterSource.Contains('IN_USE')
    ReloadOrWeaponCommand = $actionAdapterSource -match '(?i)reload'
    ActionTranslator = $commandSource.Contains('ActionAdapter::translate') -and -not $movementBody.Contains('ActionAdapter::translate')
}
$result.LiveActionBoundaryAvailable = [bool](
    $result.MovementDispatch -and
    $result.CombatCoreCall -and
    $result.ObjectiveCoreCall -and
    $result.AttackButton -and
    $result.UseButton -and
    $result.ReloadOrWeaponCommand -and
    $result.ActionTranslator)

if ($Json) {
    [pscustomobject]$result | ConvertTo-Json -Depth 4
} else {
    $result.GetEnumerator() | ForEach-Object {
        Write-Output ("{0}={1}" -f $_.Key, $_.Value)
    }
}

if ($RequireLiveActions -and -not $result.LiveActionBoundaryAvailable) {
    Write-Output "MISSING live action adapter boundary: Core combat/objective decisions do not reach attack/reload/weapon/IN_USE dispatch."
    exit 2
}

exit 0
