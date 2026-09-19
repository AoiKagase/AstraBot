# P00 Known Deviations and Blockers

## P06 perception deviations

- Core separates observed contact, believed last-known contact, and unknown
  contact. Runtime hidden actors are published without position, so
  Compatibility target selection cannot consume global entity coordinates.
- The public adapter reproduces the reference body probe order and public
  trace fraction/self-skip boundary, but ignore_glass, smoke occlusion,
  private blind state, and ReGameDLL engine trace behavior remain
  IMPLEMENTED_UNVERIFIED.
- The reference recognition queue, IsNoticable attention interval, private
  profile reaction time, and current-enemy replacement rules are not fully
  reproduced. Offline tests cover the bounded visible-contact and last-known
  model only.
- Noise position randomization uses the public observed position as an
  explicitly approximated position. Reference noise RNG callsites remain
  catalogued under RNG-CSBOT-EVENT-NOISE; no direct RNG call was added in P06.
- Perception events have an SDK-free input boundary and deterministic lifecycle
  mutations, but no public Metamod/GameDLL event feed has been claimed.
- P06 offline gates pass; live HLDS/ReHLDS differential perception and event
  acceptance were not run.

## P05 state-machine deviations

- `CompatibilityStateMachine` proves lifecycle ordering and overlay ownership,
  not the internal decision algorithm of each CSBot state.
- The runtime requests only the public C4-carrying PlantBomb lifecycle in the
  current Compatibility integration. Planted-bomb tactical choice, defuse
  reasoning, follow leader, noise, visibility, path failure, and entity-use
  transitions remain unavailable or deferred.
- Private weapon accuracy, reload, next-primary/secondary timers, and related
  weapon branches remain `UNAVAILABLE`; P05 never substitutes defaults.
- `TRANS-RNG-DEPENDENT` is explicitly blocked when the Compatibility RNG
  boundary is unavailable. No new direct random callsite was added.
- Core state traces are deterministic fixture evidence. No pinned reference
  CSBot runtime trace or live HLDS/ReHLDS state acceptance was collected.

## P04 observation deviations

- `EXACT_ENGINE_API` public player fields are not behavioral `MATCH`; CSBot
  private object semantics, timing, and lifecycle still differ.
- FOV currently observes public `entity->v.fov`, but scoped/internal zoom state
  is unavailable and no CSBot visibility/trace parity is claimed.
- Active weapon, clip, reserve ammo, reload, next attack timers, accuracy,
  weapon flags, silencer, burst, and zoom remain `UNAVAILABLE`.
- C4 possession, planted state, bomb position, and timer are public proxies or
  entity heuristics classified `INFERRED`; defusing, kit, hostage, rescue, VIP,
  round, freeze, and win state are unavailable/unknown.
- `OBS-TRACE-*` fields are `NOT_YET_IMPLEMENTED`; current visibility remains
  nearest-hostile enumeration and is `INFERRED`.
- Existing direct public readiness, bounds, and bomb-site reads remain in the
  adapter/runtime as documented legacy boundaries; no pdata or ReAPI dependency
  was added.
- Observation trace sequence is adapter-local and does not establish reference
  trace parity or live acceptance.

These are audit findings, not implementation instructions executed in P00.

## Critical

1. **Compatibility boundary is implemented but not behavioral parity.** P01 adds
   `RuntimeMode::Compatibility` as the default, the `astrabot_mode` CVar,
   `RuntimeModePolicy` isolation predicates, and observable runtime diagnostics.
   The current runtime has no connected enhanced decision implementation, so the
   existing Nav/Combat/Objective controllers remain unproven baseline candidates;
   this does not promote any row to `MATCH`.
2. **Think timing is resolved offline in P02.** AstraBot now exposes the
   reference 30Hz command and nested 10Hz full-update cadence through
   `BotTimingScheduler`; live/private-state parity remains an open evidence gate.
3. **RNG parity was absent at the P00/P02 baseline.** P03 now provides a
   CSBot-compatible engine callback boundary, scripted tape, and call-order trace
   contract. Specific production callsites remain unverified, and the roam
   controller still uses a deterministic actor/generation-derived route index,
   which differs from reference `RANDOM_*` use.
4. **Private state is unavailable.** Active weapon/ammo/recoil/reload/FOV,
   CSBot profile values, recognized-enemy queues, `CSGameState`, hostage state,
   and many player-private fields are not reachable through the current public
   boundary. Several current inputs are inferred or synthetic.
5. **The live action gate is not accepted.** Existing Windows evidence records
   team/class entry and human damage/death, but no sustained autonomous movement,
   Bot combat, or C4 event. Linux x86 live evidence and a pinned differential
   reference trace remain open.

## P02 timing update

The former per-frame/two-clock timing deviation is resolved for the offline
Compatibility Mode runtime boundary. `BotTimingScheduler` now matches the
pinned reference's nested 30Hz command and 10Hz full-update gates, ordered
reset/update/execute lifecycle, absolute `now + interval` rebasing, no
catch-up behavior, command-template persistence, and timestamp-based `msec`.
Live HLDS/ReHLDS acceptance, GameDLL-private state, and full CSBot behavior
remain unverified and are not promoted by P02.

## Major behavioral gaps

- The 13 reference state objects and `OnEnter`/`OnUpdate`/`OnExit` side effects
  are not represented by Astra's seven-state high-level machine.
- Live perception selects the nearest hostile public edict; it does not perform
  CSBot visibility, FOV, visible-part, hearing, event, or recognition-queue
  behavior.
- NAV parsing, corridor following, traversal, and stuck recovery are independent
  implementations. CSBot path costs, places, hiding/encounter spots, ladder
  details, tie breaks, and random choices are not proven equivalent.
- The runtime combat path constructs a synthetic rifle/weapon record and maps
  intents to public `IN_ATTACK`, `IN_USE`, and `reload` boundaries. This proves an
  action translation contract, not weapon behavior or damage.
- Objective integration is currently a narrow bomb carry/planted scan. Hostage,
  VIP, rescue, escape, follow, guard, chatter, and radio delivery are not live
  behavior.
- Bot manager/quota/profile buy behavior is not equivalent to
  `CCSBotManager`/`BotProfile`; profile parsing tests do not prove decision use.

## Evidence boundaries

- The fresh P00 offline baseline is configure/build + CTest 42/42 on Windows x86
  with the current working tree. It proves compilation and portable contracts
  only.
- PowerShell phase8 fixture/self-tests pass, including the action boundary check,
  but their logs are synthetic or source-boundary checks.
- Python manifest/artifact tests were not run because `py -3` could not create
  the installed Windows Store Python process. This is an environment gap, not a
  source pass/fail.
- No new HLDS/ReHLDS run was performed during P00. Existing live reports remain
  partial and are cited as historical evidence only.

## P03 RNG update

- ReGameDLL-CS contains 148 executable CSBot-related `RANDOM_*` expressions in
  24 direct bot files. The complete semantic inventory is in `RNG_MODEL.md`.
- ReGameDLL's `RANDOM_FLOAT` and `RANDOM_LONG` delegate through the engine
  callback table. AstraBot now delegates through one shared production adapter;
  it does not copy the generator or add per-Bot state.
- Scripted/tape and optional trace behavior are offline verified. No current
  AstraBot production decision consumes the Compatibility source yet, so the
  individual reference callsites remain UNVERIFIED.
- The current actor/generation-derived Nav choice remains DIFFERENT and was not
  changed into a random call in P03.
- The 21 direct chatter-purpose calls remain inventory items despite their
  visible scope because a shared global stream could shift later behavior.
- Live HLDS/ReHLDS acceptance, pinned reference RNG traces, private state, and
  full CSBot behavior remain open.
