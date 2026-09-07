# Phase 7 — Tactical Planner

Phase 7 generates tactical intent over a horizon of seconds to tens of
seconds. It chooses a tactical direction but does not generate `BotCommand`
directly.

## Goal

Generate tactical intents such as:

```text
ATTACK_SITE
DEFEND_SITE
ROTATE
RETAKE
SAVE
FLANK
LURK
SUPPORT
ENTRY
TRADE
HOLD
ESCORT
```

The Tactical Planner must remain separate from immediate Action selection.

## P7-01 — Tactical Intent Contracts

Define portable contracts including:

```text
IntentType
TargetArea
RouteStyle
RolePreference
Urgency
Reason
Validity
```

## P7-02 — Tactical Context

Build tactical context from:

- objective state;
- self state;
- teammate state;
- enemy beliefs;
- remaining time;
- an economy summary, when available; and
- bomb or hostage state.

## P7-03 — Attack / Defend

Implement the minimum tactical choices:

```text
Attack site
Defend site
Hold current area
```

## P7-04 — Rotate / Retake

Use the following evidence when deciding whether to rotate or retake:

- enemy concentration;
- bomb plant;
- teammate losses;
- route availability; and
- remaining time.

## P7-05 — Save

Decide whether to save based on remaining time, health, weapon value, and
retake feasibility. A complete economy AI is not required yet.

## P7-06 — Flank / Lurk / Support

Implement only a baseline version of flank, lurk, and support. Omniscient
flanking is prohibited; decisions must use the World Model and navigation
beliefs.

## P7-07 — Tactical Replanning

Support periodic and event-driven replanning. Triggers include:

- enemy sighting;
- bomb planted or dropped;
- teammate death;
- objective transition;
- blocked route; and
- invalidated intent.

## P7-08 — Scenario Tests

Minimum scenarios include:

```text
multiple enemies confirmed at A → Rotate
Bomb planted at B → Retake B
Retake impossible → Save
Entry player dies → Support reevaluates
```

## P7-09 — Phase 7 Gate

Record the result as:

```text
Phase 7 Offline: PASS

The P7 gate passed on the implementation tree after the tactical-planner
focused test and the canonical verification run:

- Windows portable x86 Debug: 58/58 tests passed.
- Windows Metamod-P x86 Debug: 76/76 tests passed.
- Windows Metamod-P x86 Release: adapter artifact and the six required
  undecorated exports passed.
- The focused P7 scenario test passed before the canonical run.

This is offline evidence only. Live HLDS/ReHLDS and real-device acceptance
remain post-Finish validation.
```
