# P02 — Runtime Timing and Command Pipeline Parity

## Goal

Reproduce the reference semantics of bot scheduling and fake-client command execution before tuning higher-level decisions.

Timing mismatches can make every later comparison meaningless because perception, aiming, path following and RNG branches occur on different ticks.

## Reference focus

Inspect the pinned versions of:

- `regamedll/game_shared/bot/bot.cpp`
- `regamedll/game_shared/bot/bot.h`
- `regamedll/dlls/bot/cs_bot_update.cpp`
- related fake-client/player movement entry points.

The reference currently separates a frequent command/upkeep cadence from a lower-frequency full AI update. Verify exact constants and ordering from the pinned commit rather than copying assumptions from this document.

## Tasks

1. Map current Astra frame/update scheduling from Metamod `StartFrame` or equivalent through the bot runtime and command submission.
2. Compare reference ordering for:
   - cadence gate;
   - lightweight upkeep;
   - full-think gate;
   - command reset;
   - full update;
   - command execution;
   - previous-command timestamp/msec calculation.
3. Implement equivalent scheduling semantics in compatibility mode.
4. Ensure server FPS variation does not accidentally turn “every frame” into AI behavior. Use game time scheduling equivalent to the reference.
5. Verify button reset/persistence semantics. A command flag persisting one tick too long is a parity bug.
6. Verify movement command fields: forward, side, up, buttons, view angles, impulse/msec and the actual fake-client execution API path.
7. Verify freeze/dead/not-yet-spawned cases that alter command execution in the pinned reference.
8. Ensure high-frequency aim/upkeep work can run without invoking full tactical planning each time if that matches the reference.
9. Add deterministic scheduling tests that simulate irregular frame times and assert command/full-think counts/order.
10. Add trace points for `command_tick`, `full_think`, and final command output; keep them disabled/cheap outside diagnostics.

## Required comparisons

Test at least:

- steady high FPS;
- steady low but valid FPS;
- jittered frame intervals;
- freeze period;
- spawn/death transitions;
- long frame hitch followed by recovery.

Do not “catch up” with multiple AI updates in one frame unless the pinned reference does so.

## Acceptance criteria

- compatibility scheduler semantics match the pinned reference order;
- command and full-think rates are verified from constants in `REFERENCE.md`;
- irregular-frame tests pass;
- command fields are emitted through one documented path;
- no enhanced planner work occurs on extra compatibility ticks;
- build/smoke gate passes.

## Commit

Suggested message:

`fix(parity): align compatibility think and command timing with CSBot`

Update STATUS/matrix, mark P02 complete, set P03 next, commit, stop.
