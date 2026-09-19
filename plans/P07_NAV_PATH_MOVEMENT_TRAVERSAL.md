# P07 — NAV, Pathfinding, Movement and Traversal Parity

## Goal

Make compatibility mode choose and execute routes like CSBot, including edge cases that strongly affect downstream combat timing.

## Reference focus

- `cs_bot_nav.cpp`
- `cs_bot_pathfind.cpp`
- `cs_bot_learn.cpp`
- `game_shared/bot/nav*`
- relevant state files such as MoveTo/Hide/Follow/Hunt
- base bot movement helpers in `game_shared/bot/bot.cpp`

## Critical rule

Existing Astra adaptive routing, traversal learning or “better” path selection is **enhanced-mode behavior** unless proven reference-equivalent. Do not delete it; bypass it in compatibility mode.

## Tasks

1. Verify NAV file format/loading semantics against the pinned reference.
2. Map area lookup, route types, path cost calculation and any team/hostage/safety modifiers.
3. Reproduce deterministic tie-breaking. Equal-cost route divergence is behaviorally significant.
4. Map hiding-spot selection and search range rules.
5. Map path invalidation/recompute triggers.
6. Map path following and arrival thresholds.
7. Map movement posture, walk/run, crouch, strafe, look direction and movement clearing.
8. Map ladder mounting, traversal and dismounting behavior.
9. Map jump initiation/cooldowns, gap/jump decisions and landing recovery that exist in the reference.
10. Map stuck detection, stuck resolution and path recovery.
11. Map hostage-following movement constraints where they change route/movement.
12. Determine scope of CSBot NAV learning/generation when a map lacks NAV. If baseline requires it, implement parity or record an explicit pre-baseline blocker; do not silently substitute Astra's learning algorithm.
13. Add deterministic path fixtures with fixed NAV graphs independent of a live engine.
14. Add live traversal cases for narrow corridors, stairs, ladders, jumps and door/use interactions.

## Acceptance criteria

- adaptive/learned Astra route weights cannot affect compatibility mode;
- deterministic path fixtures match reference route/area sequences for covered cases;
- movement command semantics align with P02 scheduling;
- ladder/jump/stuck behavior has targeted coverage;
- NAV/no-NAV policy is explicit;
- build/smoke gate passes.

## Commit

Suggested message:

`feat(parity): align compatibility navigation and traversal with CSBot`

Update STATUS/matrix, mark P07 complete, set P08 next, commit, stop.
