# Phase 13 — Final Integration and Live Acceptance

Phase 13 formally accepts the offline-complete AstraBot in real
HLDS/ReHLDS environments. Live acceptance remains separate from offline CI and
from the project-wide Finish decision until all required evidence is complete.

## Goal

Validate the complete runtime on real servers, maps, navigation data, combat,
planning, team coordination, persistence, and performance.

## P13-01 — Metamod Load Fix / Validation

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

## P13-02 — Real NAV Validation

Using real CS/ReGameDLL-generated `.nav` files, verify:

- load;
- nearest-area lookup;
- route search;
- traversal; and
- ladder behavior.

## P13-03 — Live Movement

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

## P13-04 — Live Perception

Verify:

- direct vision;
- occlusion;
- smoke;
- flash;
- sound;
- memory;
- belief; and
- team reports.

## P13-05 — Live Combat

Verify:

- reaction;
- aim;
- `DirectFire`;
- tap, burst, and full-auto cadence;
- reload;
- cease fire when vision is lost; and
- target replacement.

## P13-06 — Live Action / Tactical / Team AI

Verify:

- objective actions;
- cover;
- retreat;
- rotation;
- retake;
- save;
- role assignment; and
- role reassignment.

## P13-07 — Persistent Learning

Across multiple rounds and server restarts, verify:

```text
experience save
↓
reload
↓
route/action behavior changes
```

## P13-08 — Windows/Linux Runtime Acceptance

Verify runtime behavior on at least:

```text
Windows ReHLDS/ReGameDLL
Linux ReHLDS/ReGameDLL
```

## P13-09 — Performance Gate

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

## P13-10 — Stability Gate

Run long-duration tests covering:

- map rotation;
- Bot add/remove;
- disconnect/reconnect;
- round restart;
- server restart;
- database persistence;
- absence of stale identity resurrection; and
- absence of unbounded memory growth.

