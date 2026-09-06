# P3-08 — Reproducible movement evidence and separate acceptance gates

Implementation base: `51be514` (P3-08 approved plan), on
`codex/p308-offline-gate`. This change adds offline test/evidence tooling;
it does not change production navigation, public interfaces or adapter exports.
Project-wide Finish is not declared and no live server was started.

## Evidence contract

`tests/nav/simulation/scenarios.json` owns the finite expected matrix. The
portable producer covers 90 rows and the fake-host producer covers 186 rows.
Every row is actually executed twice and compares ordered traces, rather than
reusing an expected checksum. Frame intervals are 8/16/100 ms; actor matrices
exercise 1/8/16 independently owned actors. Specialized fake-engine traversal
rows use one actor. All rows have clean and map-change variants.

The Python standard-library checker requires every declared row exactly once,
independently expected row and per-actor terminal outcomes, finite simulated
frame/query/replan/trace limits, complete typed trace events and observed actor
terminals. It rejects wrong schema, fixture/manifest hashes, malformed/duplicate/
missing cases, failed replay comparison and truncated evidence. Deliberate
mutations exercise rejection paths, including false terminal success and
unrelated source/build context.

Each result records source revision, dirty content/diff hash, original fixture
hashes, actual executable SHA-256 and x86 PE/ELF header, CMake options and
compiler. Current-build validation compares those against the current checkout
and executable. Source/fixture/configuration files newer than the executable
require rebuilding; replay targets relink after CMake option changes. Context
is checked again after execution to reject concurrent source changes. This is
reproducibility provenance for normal build workflows, not binary attestation.

Raw producer output and process logs are retained in the build's
`movement-evidence/` directory; GitHub jobs upload them even after test failure.
No game assets are committed. Full traces are bounded independently of the
production 128-event actor history. `totalQueries` is measured during motion
frames, including same-frame dispatch guards; setup and ladder discovery are
counted separately. The ground cap21 is not a whole-map discovery limit.

## Scenario coverage

| Required row | Measured replay evidence and retained regression coverage |
| --- | --- |
| Spawn/goto, floor | portable floor-arrival; host simultaneous/staggered goals and per-actor identity/dispatch |
| Doorway | host door-use, door-touch, door-timeout; existing door host fault assertions |
| Stairs | host stairs-up/down with independent support-height physics |
| Narrow passage | host narrow and existing wall/steering regressions |
| Crouch | portable mixed Walk/Crouch/Walk with required Duck dispatch, crouched crossing and release; host crouch-release |
| Simple Jump | host jump/jump-failed, actual press and independent landing; retained consecutive-jump and physics-change guards |
| Ladder | host up/down/failed-dismount; map cancellation during Climb and fresh link publication |
| Dynamic blocker | host player-transient/permanent; retained expiry, avoidance and bounded replan tests |
| Stuck | one permanently stalled actor while peers arrive, transient stuck; finite recovery/attempt ownership |
| Partial/unreachable | portable typed non-execution; host unreachable observes no dispatch then an explicit reachable goal |
| Map change | each row's map variant; host retains the coordinator while map/actor identities change, stale dispatch is rejected and fresh-map navigation repeats |

Actor rows also cover cancellation, stale selector rejection, slot reuse and
staggered arrival. They check actual dispatch callbacks, later-tick command
delivery, at most one command per actor/frame, scheduled decision bounds and
bounded per-actor history. They demonstrate synthetic scheduling and isolation,
not real 16-Bot server capacity. Specialized live multi-Bot physics remains open.

## Verification

Final integrated local gates (2026-09-06): Windows x86 portable **41/41**,
Windows x86 Metamod **47/47**, WSL Debian Linux x86 **40/40**, all Debug and
warnings-as-errors. MSVC19.51.36256.0 and GCC14.2.0 were recorded in the evidence.
The Windows-only synthetic real-NAV checker accounts for the one-test
portable difference; its name does not imply real-file acceptance. The checked
SDK remains `7ec9b014f8c0a947a724644aebe34eb33706e44b`.
The fresh pre-change adapter baseline passed 44/44 Windows x86 Debug CTest.
Checker validation was developed with failing acceptance/mutation tests before
implementation; independent review exposed malformed traces, stale builds,
crouch non-execution and already-ended map cancellation, and these were fixed.

Portable observed maxima across the matrix: 325 frames, 3.3 simulated seconds,
7 queries per actor/frame, 5168 total queries, 11536 trace events and zero
replans. Windows/Linux portable maxima matched. These are measured maxima for
these fixtures only; configured upper bounds are stored independently in the
manifest. Failure fixtures have an explicit expected typed terminal, not a
blanket acceptance of any failed operation.

After final input/diagnostic trace additions, the affected Windows portable
tests passed 2/2, Windows host/movement tests 5/5 and Linux movement tests 2/2.

Reproduction follows AGENTS.md with inspector explicitly ON and Python3.9+.
Run ordinary CTest for portable Windows, pinned-SDK Windows Metamod and Linux
GCC `-m32`, all Debug with warnings-as-errors. The two replay CTest entries
invoke the checker; `tests/nav/simulation/README.md` documents individual runs.
Dedicated ASan/fuzz jobs remain unchanged. No production/linkage/export change
requires a new Release artifact for this test-only task.

This Windows-created worktree's WSL build needs `GIT_DIR` pointing at
`/mnt/h/sourcecode/003.Game/amxmodx/AstraBot/.git/worktrees/p308-offline-gate`
and `GIT_WORK_TREE` pointing at its `/mnt/h/.../.worktrees/p308-offline-gate`
checkout when running evidence checks. Set `GIT_OPTIONAL_LOCKS=0` for read-only
Git inspection. Native Linux checkouts and hosted CI do not need this mapping.
Local WSL Debian required installing Python3 and Git; multilib/GCC were present.

## Observed hosted CI, separate from this implementation

Read-only GitHub inspection on 2026-09-06 confirmed
[run 34023536620](https://github.com/AoiKagase/AstraBot/actions/runs/34023536620)
for **`db146e465b4447b8fe3347dafe06fe7048bc4474`**, branch main. All three jobs
completed successfully. This is the P3-07 baseline; it does not include this
P3-08 implementation and is not reported as its hosted pass.

| Job | Job ID | Artifact ID | SHA-256 digest |
| --- | --- | --- | --- |
| build-and-test | 101460370703 | 9986320377 | f54b8df796c1e0d08021aa4ea7e6a6b0f012cd3656aba7fbc77281008851b2f3 |
| nav-asan | 101460370874 | 9986346313 | 61255d01ffc2e80a27f0ae3e8f7c65b2e4b070264d08adf5e84afc29a79ae5f6 |
| linux-x86 | 101460370926 | 9986307784 | e5e641272ad115323f2b6520d29935947e0b89da7beb7c783c24fb922c3de355 |

Linux job steps explicitly configured GCC `-m32` Debug portable, built and ran
CTest. Artifacts were listed as unexpired at inspection. No push, remote job
trigger, merge or deployment was performed. P3-08 hosted evidence remains
pending until a separately authorized push/PR runs the updated workflow.

## Remaining acceptance

- **Real NAV: Partially validated**, limited to the prior named dust/dust2 v5
  comparisons. Generator/date/32-bit writer provenance, independent raw
  hiding/approach/encounter details and other real file versions remain open;
  see `p3-01-real-nav-compatibility.md`. Synthetic results do not close them.
- **Live: Not yet validated.** `tests/live/nav/manifest.json` enumerates all
  environment/scenario/FPS/Bot-count combinations with no invented results.
  Exact runtime/plugin/physics configuration and lawful map/NAV/start/goal
  inputs remain prerequisites. Only after explicit project-wide Finish may
  Windows/Linux HLDS/ReHLDS checks begin. Failures must reopen recorded work.
- **Finish: not assessed or declared.** Completion of this offline gate cannot
  satisfy every plan in every other phase. Pending hosted/real-file/live gates
  retain their own statuses and cannot silently become passes.
