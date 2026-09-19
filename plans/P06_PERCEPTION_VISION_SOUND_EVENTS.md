# P06 — Perception: Vision, Sound, Events and Short-Term Knowledge

## Goal

Ensure compatibility logic knows only what CSBot would know, at equivalent times, and forgets/updates it with equivalent semantics.

A smarter sensor is a compatibility bug even if the resulting bot plays better.

## Reference focus

- `cs_bot_vision.cpp`
- `cs_bot_listen.cpp`
- `cs_bot_event.cpp`
- perception-related portions of `cs_bot_update.cpp`, `cs_bot.cpp`, `cs_gamestate.*`

## Tasks

1. Map reference visibility tests, FOV constraints, line-of-sight traces and visible-body-region logic where used.
2. Map enemy-selection ordering and conditions.
3. Map last-known position/area, time-since-seen and enemy-lost behavior.
4. Map hearing/noise generation, distance/category filtering, priority, investigation and stale-noise lifetime.
5. Map game events that alter bot knowledge: gunfire, hurt/death, bomb events, hostage events, radio/team events and round transitions as applicable.
6. Ensure compatibility mode does not use omniscient global world-model facts merely because Astra's engine adapter knows them.
7. Separate **ground truth** from **bot belief** in the world model if not already separated.
8. Reset perception/memory state at exactly the appropriate lifecycle boundaries.
9. Add fixtures for:
   - target inside/outside FOV;
   - occlusion and partial visibility;
   - enemy appears/disappears;
   - heard-but-not-seen enemy;
   - multiple noises with different priorities;
   - death/round reset;
   - teammate information where the reference allows it.
10. Record any engine-trace differences that require a live test rather than unit-only verification.

## Acceptance criteria

- compatibility decisions consume bot belief rather than unrestricted ground truth;
- vision/hearing/event semantics have mapped reference behaviors;
- stale knowledge/reset behavior is tested;
- no enhanced opponent profile changes compatibility perception or target choice;
- build/smoke gate passes.

## Commit

Suggested message:

`feat(parity): align compatibility perception and event knowledge with CSBot`

Update STATUS/matrix, mark P06 complete, set P07 next, commit, stop.
