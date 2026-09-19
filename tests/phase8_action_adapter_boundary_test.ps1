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
$end = $source.IndexOf('runtime::CommandReceipt PluginRuntime::dispatchNeutralMovement(')
if ($start -lt 0 -or $end -le $start) {
    throw 'could not isolate updateManagedBotMovement adapter boundary'
}

$movementBody = $source.Substring($start, $end - $start)
$actionAdapterPath = Join-Path (Split-Path -Parent (Split-Path -Parent $AdapterPath)) 'metamod\action_adapter.hpp'
$actionAdapterSource = if (Test-Path -LiteralPath $actionAdapterPath) {
    Get-Content -LiteralPath $actionAdapterPath -Raw
} else {
    ''
}
$result = [ordered]@{
    MovementDispatch = $movementBody.Contains('inputDispatcher_.dispatchNext')
    CombatCoreCall = $source.Contains('combat::CombatController') -and $source.Contains('managedBotCombat_')
    ObjectiveCoreCall = $source.Contains('objectives::RoundObjectivePlanner') -and $source.Contains('managedBotObjectives_')
    AttackButton = $actionAdapterSource.Contains('IN_ATTACK')
    UseButton = $actionAdapterSource.Contains('IN_USE')
    ReloadOrWeaponCommand = $actionAdapterSource -match '(?i)reload'
    ActionTranslator = $source.Contains('ActionAdapter::translate')
}
$result.LiveActionBoundaryAvailable = [bool](
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
