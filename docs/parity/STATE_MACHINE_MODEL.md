# CSBot State Machine Model

## Scope and evidence

P05 establishes the Compatibility Mode state orchestration boundary. It does
not claim parity for combat, aim, navigation, buying, exact objective
reasoning, or private weapon state. The reference is ReGameDLL-CS commit
`b0889847fe6d03898be88acc9e366660efb40ab5`.

The reference has one `CCSBot`-owned current `BotState *m_state`, one instance
per ordinary state, and `m_stateTimestamp`. `AttackState m_attackState` and
`m_isAttacking` are separate fields. AstraBot maps this to one
`compat::CompatibilityStateMachine` per managed actor. The existing broad
`behavior::BehaviorStateMachine` remains in place for existing planner
contracts and is not the CSBot compatibility state source.

## Reference update architecture

`CCSBot::Update` gives the attack overlay update precedence:

```text
if (m_isAttacking)
    m_attackState.OnUpdate(this)
else
    m_state->OnUpdate(this)
```

The underlying state is retained while attacking. `Attack()` sets the attack
flag and calls `m_attackState.OnEnter` without changing the task. State change
while attacking first calls `StopAttacking()`. `StopAttacking()` calls the
attack `OnExit`, clears the flag, and may request `Idle()` when following.

## SetState semantics

The pinned `CCSBot::SetState` ordering is:

1. If the attack overlay is active, stop it.
2. Call the old state's `OnExit`.
3. Call the new state's `OnEnter` while the old state pointer is still current.
4. Publish the new state pointer.
5. Set `m_stateTimestamp = gpGlobals->time`.

There is no same-state early return. A same-state request repeats the exit and
enter lifecycle and refreshes the timestamp. AstraBot's deterministic tests
assert this ordering, including the fact that OnEnter-side effects occur before
state publication.

## State inventory

| Semantic state | Reference lifecycle and side effects | Astra mapping | Internal behavior |
|---|---|---|---|
| `STATE-IDLE` | OnEnter destroys path and sets `SEEK_AND_DESTROY`; OnUpdate selects tasks/objectives | `CompatibilityStateMachine::Idle` | Deferred; task selection is not CSBot-complete |
| `STATE-BUY` | Buy-state initialization/update/exit owns economy decisions | `CompatibilityStateMachine::Buy` | Blocked/deferred; no economy parity |
| `STATE-DEFUSE-BOMB` | Defuse update; OnExit sets `SEEK_AND_DESTROY` and clears look target | `CompatibilityStateMachine::DefuseBomb` | Blocked by objective/private observations |
| `STATE-ESCAPE-BOMB` | OnEnter destroys path; update chooses escape movement | `CompatibilityStateMachine::EscapeFromBomb` | Deferred to objective/NAV phases |
| `STATE-FETCH-BOMB` | OnEnter destroys path; update fetches loose bomb | `CompatibilityStateMachine::FetchBomb` | Blocked by exact objective state |
| `STATE-FOLLOW` | OnEnter destroys path and resets follow timers; update tracks leader | `CompatibilityStateMachine::Follow` | Blocked by leader/vision observations |
| `STATE-HIDE` | Update holds a hiding task; OnExit clears look target | `CompatibilityStateMachine::Hide` | Hiding-spot/NAV behavior deferred |
| `STATE-HUNT` | OnEnter sets `SEEK_AND_DESTROY` and destroys path | `CompatibilityStateMachine::Hunt` | Hunt selection/vision deferred |
| `STATE-INVESTIGATE-NOISE` | Update consumes delayed noise and can leave the state | `CompatibilityStateMachine::InvestigateNoise` | Blocked by live sound/perception feed |
| `STATE-MOVE-TO` | Update follows a supplied goal/path and handles failure | `CompatibilityStateMachine::MoveTo` | Route and locomotion behavior deferred |
| `STATE-PLANT-BOMB` | Update performs plant sequence; OnExit sets `GUARD_TICKING_BOMB` and clears look target | `CompatibilityStateMachine::PlantBomb` | Public C4 carrying proxy only; full plant behavior deferred |
| `STATE-USE-ENTITY` | Update uses the selected entity; OnExit clears look target | `CompatibilityStateMachine::UseEntity` | Entity-use observation/action deferred |
| `STATE-ATTACK-OVERLAY` | Separate attack instance; OnEnter destroys path; update owns updates while active; OnExit ends overlay | `attackOverlayActive_` plus `AttackOverlay` trace identity | Combat algorithm intentionally unchanged |

All twelve ordinary state instances are owned by the per-bot machine. The
attack overlay is intentionally not returned as the ordinary `state()` value.

## Transition inventory

| Transition ID | Source | Destination | Trigger/priority | Status |
|---|---|---|---|---|
| `TRANS-INITIAL-IDLE` | none | Idle | bot initialization | implemented, offline verified |
| `TRANS-EXPLICIT-STATE-CHANGE` | any ordinary state | requested ordinary state | compatibility request | implemented, offline verified |
| `TRANS-IDLE-TO-MOVE-TO` | Idle | MoveTo | deterministic request | implemented, offline verified |
| `TRANS-MOVE-TO-HUNT` | MoveTo | Hunt | enemy-visible request | implemented, offline verified |
| `TRANS-ANY-TO-ATTACK-OVERLAY` | any ordinary state | overlay active | existing combat fire intent | represented; live trace not collected |
| `TRANS-ATTACK-STOP` | overlay active | overlay inactive | attack end or ordinary state change | implemented, offline verified |
| `TRANS-DEATH-TO-IDLE` | any | Idle | death lifecycle | implemented, offline verified |
| `TRANS-RESPAWN-TO-IDLE` | any | Idle | spawn/respawn lifecycle | implemented, offline verified |
| `TRANS-ROUND-RESET-TO-IDLE` | any | Idle | round generation change | implemented, offline verified |
| `TRANS-RNG-DEPENDENT` | any | requested state | reference random branch | explicitly blocked when RNG unavailable; no default consumed |

Transition trace records include state sequence, actor, previous/current state,
transition ID, reason, timestamp, Full Update sequence, overlay status, task,
and side effect. State updates are accepted only from the existing Full Update
cadence; the P02 scheduler is not modified.

## Observation and RNG boundaries

The runtime uses P04 `CompatibilityObservation` metadata. A request depending
on unavailable observation data returns `BlockedByObservation`; a random branch
with unavailable RNG returns `BlockedByRng`. P05 does not synthesize weapon
accuracy, reload, next-attack timers, or other private weapon fields.

The current runtime integration wires the public C4-carrying proxy to the
PlantBomb lifecycle when Compatibility Mode and team evidence permit it. Exact
planted-bomb tactical reasoning, defuse-kit state, visibility, leader state,
noise, and route failure remain documented blockers.

## Lifecycle and mode isolation

Each managed FakeClient has an independent machine, Full Update sequence,
round-generation marker, and dead/respawn edge. Disconnect, map reset, and
slot reuse construct a fresh actor-owned machine. Enhanced planner policy is
not consulted for Compatibility state requests; existing enhanced capabilities
remain governed by `RuntimeModePolicy`.

## Validation boundary

`tests/compat_state_machine_tests.cpp` covers initial state, exact SetState
ordering, same-state requests, Full Update gating, multi-step transitions,
attack overlay activation/update/exit, state change during attack, unavailable
observation/RNG blockers, actor isolation, and death/respawn lifecycle. This is
deterministic Core evidence, not live HLDS/ReHLDS state parity.

P05 result remains `PARTIAL`: the orchestration boundary is implemented and
offline verified, while most state internals, private observations, and a
pinned differential runtime trace remain open. P06 is not started.
