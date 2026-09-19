# P00 — Freeze Reference and Audit Current AstraBot

## Goal

Create a stable target and a durable map of the current codebase before changing behavior.

This plan is mostly analysis/documentation. Do not start “fixing” parity mismatches yet unless a tiny documentation/build helper is required to complete the inventory.

## Required input

- current AstraBot working tree
- ReGameDLL_CS repository/source access
- `README_FIRST.md`
- `ROADMAP.md`

## Tasks

1. Record the current AstraBot commit SHA and working-tree state.
2. Resolve and pin the exact ReGameDLL_CS commit used as the behavioral reference. Do not write `master` only.
3. Record reference build options that can alter bot behavior, especially `REGAMEDLL_FIXES`, `REGAMEDLL_ADD`, bot-related compile options and runtime CVAR assumptions.
4. Record the intended server stack for live parity: HLDS or ReHLDS version, GameDLL, Metamod/Metamod-P version, 32-bit requirements and toolchain.
5. Inventory every CSBot-related reference file under:
   - `regamedll/game_shared/bot/`
   - `regamedll/dlls/bot/`
   - `regamedll/dlls/bot/states/`
   - transitive weapon/player/GameRules fields actually read by CSBot.
6. Inventory current AstraBot modules and tests. At minimum search for existing runtime orchestration, fake-client lifecycle, world model/perception, movement, NAV/pathfinding, ladders, jump handling, combat/aim, weapon selection/buying, objectives, radio/team planning, opponent profile, adaptive route/traversal learning and replay/differential tests.
7. Create `docs/parity/REFERENCE.md` containing the pinned reference and environment.
8. Create `docs/parity/SOURCE_MAP.md` mapping each ReGameDLL source area/symbol group to current Astra source files/symbols.
9. Create `docs/parity/PARITY_MATRIX.md` using the supplied template and populate as much as can be determined without edits.
10. Create `docs/parity/STATUS.md` using the supplied template.
11. Create `docs/parity/KNOWN_DEVIATIONS.md` with an empty/none-approved state. Do not classify unfinished work as a deviation.
12. Copy/adapt the supplied trace schema to `docs/parity/TRACE_SCHEMA.md`.
13. Identify every existing Astra-only advanced behavior and mark whether it currently participates in normal decisions. Examples may include adaptive routing, traversal learning, opponent profiles, tactical planner/team director and other learned heuristics.
14. Produce a short `P00 findings` section in `STATUS.md`: top parity risks, current live-runtime blockers, and modules most likely to already satisfy the reference.

## Mandatory reference facts to verify, not assume

- the command-think and full-think cadence and ordering in `CBot::BotThink`;
- the command execution path in `CBot::ExecuteCommand`;
- the exact CSBot state files present in the pinned reference;
- the role of the special attack overlay versus the normal state pointer;
- profile/manager/chatter source locations;
- build flags that change bot semantics.

## Do not

- rewrite the planner;
- delete advanced Astra modules;
- rename large portions of the project;
- introduce a new architecture based only on this roadmap without inspecting current code;
- claim a feature is missing merely because its name differs from ReGameDLL.

## Acceptance criteria

- exact reference SHA is recorded;
- current Astra SHA is recorded;
- all reference CSBot files have an entry in `SOURCE_MAP.md`;
- every major parity domain has at least one row in `PARITY_MATRIX.md`;
- current advanced Astra behaviors are identified;
- no material gameplay behavior has been changed;
- normal build/smoke test still passes.

## Commit

Suggested message:

`docs(parity): freeze CSBot reference and map current implementation`

Update `docs/parity/STATUS.md`, mark P00 complete, name P01 as the next plan, commit, then stop.
