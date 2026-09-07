# Phase 11 — AMX Mod X API

Phase 11 exposes AstraBot as an optional external-control bridge without
making AMX Mod X a required dependency of the core.

## Goal

Keep the dependency direction:

```text
AstraBot Core
↑
Metamod Runtime
↑
AMXX Bridge
```

## P11-01 — AMXX Bridge Boundary

Define the bridge boundary so AMX Mod X types and lifecycle details do not
leak into Core.

## P11-02 — Basic Natives

Candidate natives:

```pawn
astrabot_add(...)
astrabot_remove(...)
astrabot_get_role(...)
astrabot_set_role(...)
astrabot_get_intent(...)
astrabot_set_goal_area(...)
astrabot_get_current_area(...)
astrabot_pause_ai(...)
```

## P11-03 — Observability Natives

Candidate observability natives:

```pawn
astrabot_get_enemy_confidence(...)
astrabot_get_action(...)
astrabot_get_route_cost(...)
astrabot_get_experience(...)
```

## P11-04 — Forwards

Candidate forwards:

```pawn
astrabot_role_changed(...)
astrabot_intent_changed(...)
astrabot_enemy_acquired(...)
astrabot_enemy_lost(...)
astrabot_goal_reached(...)
astrabot_stuck(...)
```

## P11-05 — Safety

The Core must remain safe when the AMXX side supplies:

- stale `PlayerId`;
- an invalid goal;
- an invalid role; or
- a cross-map handle.

## P11-06 — Phase 11 Gate

Verify Windows and Linux builds together with basic AMX Mod X integration.

