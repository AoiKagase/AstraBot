# AstraBot CSBot Parity Roadmap Pack

## Purpose

This package is intended to be copied directly into the AstraBot repository and handed to Codex.

The immediate goal is **not** to make AstraBot smarter than CSBot. The immediate goal is to establish a reproducible, testable **CSBot-compatible baseline** implemented as original AstraBot code. Once that baseline is frozen, Astra-specific intelligence can be layered on top without losing the ability to distinguish an intentional enhancement from a compatibility regression.

The behavioral reference is ReGameDLL_CS CSBot. AstraBot must not copy ReGameDLL source code into AstraBot. Use ReGameDLL only as a behavioral/reference oracle: inspect inputs, state transitions, timing, side effects, outputs, and externally visible behavior, then implement equivalent behavior independently.

## How Codex should use this pack

Do **not** read every plan at once. That defeats the point of the split plans and increases context pressure.

For each session/phase, load only:

1. `README_FIRST.md`
2. `ROADMAP.md`
3. `docs/STATUS.md` if it exists
4. the single current `plans/Pxx_*.md`
5. source files required by that plan

At the end of every plan:

- run only the tests required by that plan plus the project smoke/build gate;
- update `docs/STATUS.md`;
- update parity mapping documents created by P00;
- commit the phase as one coherent commit unless a build-fix follow-up commit is clearly necessary;
- do **not** push unless explicitly instructed by the user;
- do not begin the next plan automatically.

## Non-negotiable project rules

- Compatibility mode must remain a first-class runtime mode throughout the project.
- Existing Astra-specific intelligence must not be deleted just because it differs from CSBot; it must be isolated/gated so compatibility mode cannot use it.
- Do not perform broad refactors unrelated to the active plan.
- Do not “improve” CSBot behavior while working on parity. A strange ReGameDLL behavior is still the compatibility target unless explicitly documented otherwise.
- Do not hide mismatches by widening test tolerances. Any tolerance must have a physical/engine reason and be documented.
- Prefer deterministic fixtures and differential traces over subjective “it looks right” playtesting.
- Do not add redundant tests merely to increase coverage. Add tests that detect a concrete parity regression.
- Preserve current working behavior unless the reference proves it is incompatible.
- Never infer parity from function names. Compare observable semantics.

## C/C++ editing discipline

Follow repository-local instructions first (`AGENTS.md`, `.editorconfig`, existing style). When no local rule exists, use the project owner's preferred conventions:

- one statement per line;
- spaces around binary operators;
- a space after commas;
- opening and closing braces on their own lines;
- real tabs for indentation, display width 4;
- LF line endings;
- UTF-8 without BOM;
- final newline required;
- no trailing whitespace;
- avoid magic numbers; name compatibility constants and cite their reference origin in comments/documents where useful.

Do not reformat untouched code simply to enforce style.

## Reference source roots

ReGameDLL_CS:

- `regamedll/game_shared/bot/`
- `regamedll/dlls/bot/`
- `regamedll/dlls/bot/states/`
- any player/weapon/GameRules code transitively read by CSBot

Known high-value reference files include:

- `game_shared/bot/bot.cpp`, `bot.h`, `bot_constants.h`
- `game_shared/bot/bot_manager.*`, `bot_profile.*`, `bot_util.*`
- `dlls/bot/cs_bot.*`
- `dlls/bot/cs_bot_init.*`
- `dlls/bot/cs_bot_update.cpp`
- `dlls/bot/cs_bot_statemachine.cpp`
- `dlls/bot/cs_bot_vision.cpp`
- `dlls/bot/cs_bot_listen.cpp`
- `dlls/bot/cs_bot_event.cpp`
- `dlls/bot/cs_bot_weapon.cpp`
- `dlls/bot/cs_bot_nav.cpp`
- `dlls/bot/cs_bot_pathfind.cpp`
- `dlls/bot/cs_bot_learn.cpp`
- `dlls/bot/cs_bot_radio.cpp`
- `dlls/bot/cs_bot_manager.*`
- `dlls/bot/cs_bot_chatter.*`
- `dlls/bot/cs_gamestate.*`
- all files under `dlls/bot/states/`

Reference repository: https://github.com/rehlds/ReGameDLL_CS
AstraBot repository: https://github.com/AoiKagase/AstraBot
