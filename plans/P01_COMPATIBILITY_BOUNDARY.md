# P01 — Establish Compatibility / Enhanced Mode Boundary

## Goal

Make it mechanically possible to run AstraBot with every Astra-only intelligence enhancement disabled, without deleting those features.

## Preconditions

- P00 complete.
- `REFERENCE.md`, `SOURCE_MAP.md`, `PARITY_MATRIX.md`, `STATUS.md` exist.

## Design rule

Compatibility mode is the default test oracle mode. Enhanced mode is not allowed to leak into it through shared weights, caches, persistent profiles or route costs.

## Tasks

1. Inspect how runtime configuration/CVARs are currently represented. Reuse existing configuration machinery rather than inventing a second system.
2. Introduce one explicit mode selection with stable semantics, e.g. `compatibility` and `enhanced`. Naming may follow existing conventions.
3. Enumerate all known Astra-only decision modifiers from P00 and gate them.
4. In compatibility mode, ensure each modifier has either:
   - no effect;
   - a reference-equivalent implementation;
   - or an explicit parity blocker recorded in the matrix.
5. Keep data collection separate from decision influence where useful. For example, an opponent profile may continue collecting diagnostics in compatibility mode only if that collection cannot alter RNG consumption, timing or behavior.
6. Add a small test that proves a representative enhanced-only feature cannot alter a compatibility-mode decision/output.
7. Add mode to diagnostics/version/status output so traces always show which mode produced them.
8. Update `SOURCE_MAP.md` and `PARITY_MATRIX.md` with the compatibility boundary.

## Special warning: RNG/timing leakage

A disabled enhanced feature must not consume compatibility RNG or change scheduling merely because its result is later ignored. “Compute then discard” can still break parity. Prefer bypassing the enhanced code path before side effects.

## Do not

- implement new intelligence;
- remove adaptive route/traversal/opponent-profile code;
- refactor the whole planner just to add the switch;
- set enhanced mode as a dependency of base movement or fake-client creation.

## Acceptance criteria

- runtime mode is explicit and observable;
- compatibility mode bypasses all documented enhanced-only decision influence;
- at least one regression test proves isolation;
- existing enhanced mode remains buildable/runnable to the extent it worked before;
- build/smoke gate passes.

## Commit

Suggested message:

`feat(parity): isolate CSBot compatibility mode from Astra enhancements`

Update STATUS, mark P01 complete, set P02 next, commit, stop.
