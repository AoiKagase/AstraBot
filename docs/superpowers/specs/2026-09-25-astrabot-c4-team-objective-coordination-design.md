# AstraBot C4 and Team Route Coordination Design

Status: awaiting written specification review; the recommended approach was selected in chat.

## Goal

Make Compatibility Bots move reliably through the round, assign distinct useful routes to teammates, and complete Counter-Strike bomb objectives:

- a Terrorist carrying C4 reaches a reachable bombsite and plants it;
- when C4 is dropped, the nearest reachable living Terrorist without C4 is assigned to retrieve it and then plant it;
- after the bomb is planted, one nearest reachable living Counter-Terrorist is assigned to defuse it;
- other Bots continue on distinct routes instead of following the same roaming corridor.

## Current evidence

- `MovementExecutionGate` classifies every available `maxspeed` in `(0, 1]` as `RoundFreeze`. It zeros movement inputs. The live log records Bot 1 at `maxspeed=1.0`, unchanged position and zero velocity, while its navigation intent requests forward movement.
- The same live interval assigns Bots 2 and 4 the same roam target Area 8 and the same Area 42-to-8 link.
- The current action proposal path emits Plant or Defuse only within 96 units of the objective point. The carrier remains far from the selected site, so this run contains no plant action or plant event.
- The adapter already translates Plant to C4 selection and attack input, and Defuse to use input. The missing evidence is successful movement to the objective, sustained action, and a confirmed game outcome.
- Current public observations cover C4 possession and planted C4. A dropped-C4 observation and nearest-teammate assignment are not present.

## Design

### 1. Recover movement after the actual freeze window

- Use the public frozen flag and round lifecycle timing to determine whether the round is in its freeze period. `maxspeed <= 1` is supporting evidence during that period, not a permanent freeze state.
- After the tracked freeze period ends, allow normal movement and restore the managed Bot's normal movement speed if the GameDLL left `maxspeed` at the freeze value.
- Keep objective actions that intentionally stop movement separate from round-freeze state. Plant and Defuse must not have their action buttons removed by movement recovery.
- Preserve command scheduling and `msec`; do not treat a stale low `maxspeed` value as proof that a round is still frozen.

### 2. Assign round objectives and roaming routes across the team

Add a round-scoped, SDK-free team assignment component that receives validated actor, objective, and navigation observations and produces stable assignments.

- A current C4 carrier owns the Plant assignment and keeps the selected reachable bombsite until planting succeeds or the assignment becomes invalid.
- For a dropped bomb, select the living, eligible Terrorist with the lowest reachable NAV route cost to the bomb. Break equal-cost ties by stable actor identity. When NAV data is unavailable, use bounded geometric distance and report that fallback.
- For a planted bomb, select the living Counter-Terrorist with the lowest reachable NAV route cost to the bomb. A defuser kit is not required for eligibility because that fact is not available through the current public observation boundary.
- Other living teammates receive distinct roam targets. Prefer assignments with different target Areas and less shared initial corridor links. Retain each assignment until completion, invalidation, or a route failure that requires reassignment. If the NAV offers no distinct feasible route, permit a shared route and record the fallback reason.
- Release assignments on death, disconnect, actor-generation change, round change, map change, objective completion, or confirmed path failure.

### 3. Observe and execute C4 objectives through the public adapter

- Extend the Metamod observation boundary to identify a dropped C4 using validated public entity classname/model, origin, and lifecycle data. Pass an availability-qualified, SDK-free dropped-bomb observation into Core. If the entity cannot be identified reliably, leave the observation unavailable and do not invent an assignment.
- Reconcile dropped, carried, planted, and completed states using actor possession and observed entity transitions. Associate state with map, round, and entity generations so a stale entity cannot retain an assignment.
- A Bot assigned to retrieve dropped C4 navigates to the entity. GameDLL touch/pickup performs the actual pickup; observed C4 possession confirms success and transfers it to the Plant assignment.
- The assigned carrier travels to the selected BombTarget. Within the validated plant region, select C4 once on action entry, stop analog movement, and hold the existing attack input until planted state is observed. Release the action on success, death, state change, or invalid target.
- The assigned Counter-Terrorist travels to the planted-bomb point. Within the validated defuse region, stop analog movement and hold the existing use input until the planted objective is completed or the assignment is invalidated.
- Do not issue Plant or Defuse to every teammate. Non-assigned teammates retain separate routes.

### 4. Preserve architecture and mode boundaries

- Compatibility Core remains SDK-free; entity scanning, classname/model validation, and GameDLL input translation stay in the Metamod adapter.
- Use public engine/edict observations only. Keep private GameDLL weapon, bomb, or player state unavailable.
- Keep the change within the requested Compatibility movement, team route, and C4 objective behavior. Do not extend Enhanced tactics or general combat/aim/weapon behavior.
- Preserve unrelated worktree artifacts and existing build/deployment evidence.

## Diagnostics

Under the existing profile diagnostics setting, correlate actor generation and frame with freeze evidence, `maxspeed`, objective state and entity identity, assigned actor, target, route target and first link, command buttons, before/after origin and velocity, and completion or reassignment reason. Ordinary logging remains unchanged.

## Acceptance criteria

Offline behavior coverage should establish:

- stale `maxspeed=1` does not keep a Bot frozen after the tracked freeze period, while a real freeze still neutralizes movement;
- teammates receive distinct destinations and avoid a shared initial route when alternatives are reachable, then release reservations on lifecycle changes;
- dropped C4 is assigned to the nearest reachable eligible Terrorist with deterministic ties, and assignment transfers only after possession is observed;
- Plant and Defuse inputs are held only by the assigned actor inside the corresponding region and stop after objective completion or invalidation.

Live acceptance remains a separate gate. Use fresh offset-scoped logs and verify the deployed DLL SHA-256 matches the build before judging a run. Require evidence of physical movement and NAV progress, dropped-bomb pickup and observed possession, `Planted_The_Bomb`, and `Defused_The_Bomb`. Input or assignment logs alone do not pass these criteria.

## Open implementation detail

The Metamod adapter must confirm the actual dropped-C4 public entity representation used by the configured ReGameDLL-CS server. The implementation should support only representations verified against that runtime and leave other forms unavailable.
