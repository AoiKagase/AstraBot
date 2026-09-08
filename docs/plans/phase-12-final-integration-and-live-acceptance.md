# Phase 12 — Final Integration and Live Acceptance

Phase 12 formally accepts the offline-complete AstraBot in real
HLDS/ReHLDS environments. Live acceptance remains separate from offline CI and
from the project-wide Finish decision until all required evidence is complete.

## Goal

Validate the complete runtime on real servers, maps, navigation data, combat,
planning, team coordination, persistence, and performance.

Before live work, the adapter must pass the offline Runtime gate. The
`RuntimeOrchestrator` owns one team director, slot-scoped tactical/action/
combat state, map-session learning state, and bounded diagnostics. Its fixed
order is perception, learning, team, tactical, action, combat, navigation,
then one-command composition. Cadence is 1 second / 200 ms / 100 ms, with
event-driven runs and expiry-bound reuse of valid decisions only.

Runtime remains single-primary. Invalid or generation-mismatched input is
fail-closed and never dispatches an old attack or movement command. Live
HLDS/ReHLDS, performance, and long-duration stability evidence is post-Finish
validation, not an offline gate.

## Offline Runtime Gate

Before any live task in this phase, an adapter-owned input provider must
convert the current `PlayerRegistry`, `BotAgentRegistry`, `WorldModel`,
`NavConsole`, combat observation, and explicit objective/economy DTOs into
`RuntimeActorInput` values. The provider is synchronous and borrowed for one
frame. A missing or invalid DTO is a skip/no-op for that actor; it never grants
permission to reuse an old command. When no provider is installed, the
orchestrator intentionally performs no actor dispatch.

The canonical offline gate is:

```text
tools/verify-canonical.ps1 -Profile All
```

This gate proves the bounded value-level orchestration only. It does not prove
real HLDS/ReHLDS behavior, performance, stability, or project-wide Finish.

## P12-01 — Metamod Load Fix / Validation

Address the known live issue:

```text
Meta_Attach() returns 0
```

Minimum validation:

- diagnostics for `Meta_Attach` failure;
- timing of User Message ID resolution;
- newapi and hook-table requirements;
- plugin load and reload; and
- map change.

## P12-02 — Real NAV Validation

Using real CS/ReGameDLL-generated `.nav` files, verify:

- load;
- nearest-area lookup;
- route search;
- traversal; and
- ladder behavior.

## P12-03 — Live Movement

Verify on a real server:

```text
goto area
walk
door
stairs
crouch
jump
ladder
stuck/recovery
```

## P12-04 — Live Perception

Verify:

- direct vision;
- occlusion;
- smoke;
- flash;
- sound;
- memory;
- belief; and
- team reports.

## P12-05 — Live Combat

Verify:

- reaction;
- aim;
- `DirectFire`;
- tap, burst, and full-auto cadence;
- reload;
- cease fire when vision is lost; and
- target replacement.

## P12-06 — Live Action / Tactical / Team AI

Verify:

- objective actions;
- cover;
- retreat;
- rotation;
- retake;
- save;
- role assignment; and
- role reassignment.

## P12-07 — Persistent Learning

Across multiple rounds and server restarts, verify:

```text
experience save
↓
reload
↓
route/action behavior changes
```

## P12-08 — Windows/Linux Runtime Acceptance

Verify runtime behavior on at least:

```text
Windows ReHLDS/ReGameDLL
Linux ReHLDS/ReGameDLL
```

## P12-09 — Performance Gate

Target:

```text
32-slot server
maximum 16 Bots
server FPS remains within a practical range
```

Measure:

- perception cost;
- local navigation;
- A*;
- combat;
- planner;
- team director; and
- experience updates.

## P12-10 — Stability Gate

Run long-duration tests covering:

- map rotation;
- Bot add/remove;
- disconnect/reconnect;
- round restart;
- server restart;
- database persistence;
- absence of stale identity resurrection; and
- absence of unbounded memory growth.
