# P12 — Live Parity Matrix and CSBot Baseline Release

## Goal

Prove that deterministic unit/replay parity survives the real GoldSrc/ReHLDS + GameDLL + Metamod environment, then freeze a baseline before enhanced work resumes.

## Preconditions

- P00-P11 complete or blockers explicitly resolved/approved.
- no known Metamod load, fake-client lifecycle, spawn or basic runtime crash blocker.

## Live matrix

Use representative stock maps/assets available in the test install. Suggested coverage, adjusted to what is actually installed:

- `de_dust2` — baseline bomb/navigation/combat;
- `de_nuke` or another ladder/vertical map — ladders/vertical traversal;
- `cs_office` — hostage/use/escort;
- `as_oilrig` — VIP scenario if available;
- an `es_` map such as a stock/available escape map — escape scenario if available;
- one map with narrow paths/jump/stuck stress known to expose Astra issues.

## Tasks

1. Record exact server/runtime versions in `REFERENCE.md` and STATUS.
2. Verify plugin load/unload and map-change lifecycle.
3. Verify bot add/join/team/spawn/death/round restart/map change repeatedly.
4. Verify multiple bots, not only one bot.
5. Run compatibility mode with enhanced features disabled and trace mode available.
6. Cover buy, move, encounter, kill/death, reload, weapon switch/pickup, bomb carry/drop/fetch/plant/defuse/escape, hostage interaction, radio/follow, ladder/jump/stuck recovery.
7. Compare representative live traces against reference traces. Not every second of a long match must be byte-identical; investigate the earliest meaningful divergence until explained.
8. Re-run deterministic suite after live fixes.
9. Resolve every remaining matrix row into `MATCH-TRACE`, `MATCH-LIVE`, or an explicitly owner-approved `DEVIATION`. `MISSING`/`UNMAPPED` is not baseline-complete.
10. Verify compatibility mode does not access persistent opponent-profile/adaptive-learning state.
11. Create a baseline release note documenting:
    - pinned ReGameDLL SHA/options;
    - Astra baseline SHA;
    - supported server stack;
    - verified scenarios/maps;
    - approved deviations, ideally none;
    - known non-behavioral limitations.
12. Create/tag the baseline only after tests are clean. Do not push/tag remote without explicit user authorization.

## Acceptance criteria

- repeatable real-server session with multiple bots is stable;
- core live behavior domains have no unexplained parity divergence;
- compatibility differential suite passes after live fixes;
- all enhanced-only decision modifiers remain inert;
- baseline documentation is complete;
- project owner can reproduce the live smoke sequence from instructions.

## Commit

Suggested message:

`release(parity): freeze CSBot-compatible AstraBot baseline`

Update STATUS, mark P12 complete. Do not start P13 until the user explicitly accepts the baseline.
