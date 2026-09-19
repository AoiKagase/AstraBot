# CSBot Parity Status

## P00 result

`PASS` for the P00 audit artifact set. This does not mean the CSBot-compatible
baseline passes. Compatibility mode remains `not established`.

P00 was analysis/documentation only. No gameplay implementation, Astra extension,
or reference source was changed.

## Frozen baseline

- AstraBot audit commit: `9788c245f5017567c60c35fa3afe474d56d1a349`
- ReGameDLL-CS reference: `master` at `b0889847fe6d03898be88acc9e366660efb40ab5`
- Current Astra worktree: dirty before P00; pre-existing changes preserved
- Reference worktree: dirty, but all comparison claims use the pinned commit
- Current fresh DLL: `build-action-adapter-x86-1451/astrabot_mm.dll`, SHA-256
  `3d2bda1bd90065ca5553d0b4de13e9d6e32f03c59f099981c705d5e81dcc4d0f`

## Baseline gates

| Gate | Result | Evidence |
|---|---|---|
| Configure | PASS | CMake reconfigured `build-action-adapter-x86-1451` |
| Full x86 build | PASS | VS 2026 HostX86/x86, NMake, Debug, all targets |
| CTest | PASS | 42/42, 0 failed, 3.23 seconds |
| Phase8 PowerShell fixture/self-tests | PASS | live-log, objective, action-boundary checks |
| Python manifest/artifact tests | NOT RUN | `py -3` could not create the installed Python process |
| New live HLDS/ReHLDS run | NOT RUN | P00 did not start a new live server run |
| Existing live acceptance | PARTIAL | Windows join/human damage/death; autonomous action/C4/Linux remain open |

## Phase ledger

| Plan | Status | Result / blocker |
|---|---|---|
| P00 | complete | reference frozen; source map, matrices, observations and blockers recorded |
| P01 | pending | establish an explicit compatibility/enhanced boundary after review of P00 |
| P02-P12 | pending | timing through live parity depend on the preceding contracts and evidence |
| P13 | deferred | enhanced intelligence remains downstream of the baseline |

The repository's existing `.planning/STATE.md` is not rewritten by this audit;
its Phase 8 live-gap state remains authoritative for that separate workstream.

## P01 readiness

`NO` for claiming or implementing parity behavior immediately: the critical
timing, RNG, private-state, visibility, state-machine, and mode-gating blockers
remain. `YES` only to begin P01 planning/review using this artifact set as the
entry point.

## Required next evidence

1. Define and prove compatibility-mode isolation before enabling any Astra-only
   planner, route memory, profile adaptation, team director, or learning behavior.
2. Add a deterministic RNG record/replay boundary and pin reference call order.
3. Map public/private observations and choose explicit `exact`, `delayed`,
   `inferred`, or `unavailable` policies.
4. Establish timing/command traces for the reference and Astra paths.
5. Keep real-server movement, combat, C4, Linux x86, and lifecycle acceptance
   separate from offline tests.
