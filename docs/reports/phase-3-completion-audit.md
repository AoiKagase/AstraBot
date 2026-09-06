---
audited: 2026-09-06
source_revision: b9149c0560d365215d29680de94494fd90e21730
scope: Phase 3 completion and post-Finish live acceptance readiness
status: gaps_found
offline_implementation: complete
offline_tasks: 8/8
hosted_ci: passed_at_source_revision
real_nav: partially_validated
live_readiness: not_ready
live_acceptance: not_yet_validated
project_wide_finish: not_confirmed
---

# Phase 3 completion audit and live acceptance readiness

## Decision and scope

**P3-01 through P3-08 implementation and applicable offline verification are
complete. Live acceptance is not ready to execute and is not accepted.** The
combined audit has `gaps_found` because the live prerequisites below remain
open; this does not reopen already verified offline controllers.

Audited source: main `b9149c0`, after fast-forward integration. This is a
documentation-only audit of the existing eight tasks, not a new P3 task or a
production implementation. No server, deployment, remote workflow, or Finish
transition was started. Read-only GitHub inspection discovered an existing
successful push run for the audited revision.

The repository uses `docs/plans/` and slice reports, not a GSD `.planning/`
milestone/REQUIREMENTS/SUMMARY hierarchy. The audit therefore cross-references
the existing task acceptance criteria, implementation reports, and current
source/executable evidence. Missing GSD filenames are not invented project
requirements. Integration was inspected inline. The code-review graph returned
no architecture communities; fresh FocalSpan and scoped source reads supplied
the missing context.

## Requirements and completion evidence

The authority is the [Phase 3 plan](../plans/phase-3-nav-movement.md), together
with [architecture](../architecture.md) and [AGENTS.md](../../AGENTS.md).
Every task has implementation, report and registered-test evidence. Earlier
slice reports retain historical statements about work that later slices closed.

| Task | Required offline behavior / current implementation | Closing evidence and retained tests | Verdict |
| --- | --- | --- | --- |
| P3-01 | Bounded inspector; owned route/session identity; grounded current area; console goto/status/cancel; non-executable partial/unreachable results | [Session](p3-01-route-session.md), [console](p3-01-nav-console.md), [real-file comparison](p3-01-real-nav-compatibility.md); `astrabot.nav.inspection`, `inspection_cli`, `session`, `fake_client` | Offline complete; real-file sub-gate partial |
| P3-02 | Selected-edge corridor/portal constraints, exact external identity, bounded look-ahead, one enter/terminal and unsupported traversal failure | [Corridor/lifecycle](p3-02-corridor.md); `astrabot.nav.corridor`, `primitive`, `walk` | Offline complete |
| P3-03 | Supported Walk, 25 Hz decisions, per-frame Motor/later-tick transport, stale stop, ordinary use/touch doors, stairs, wall/narrow clearance | [Host wiring](p3-03-walk-host.md), [stairs](p3-03-stairs.md), [use door](p3-03-door-host.md), [touch door](p3-03-touch-doors.md), [wall/narrow closure](p3-03-wall-narrow.md); `astrabot.nav.walk`, `ground_probe`, `intent_pump`, `steering`, `door_wait`, `astrabot.motor`, `astrabot.fake_client` | Offline complete |
| P3-04 | Per-player entity/join/route/queue ownership, deterministic yield/avoid, expiring directed-edge overlay and one carried automatic retry | [Host seam](p3-04-player-host.md), [NAV sessions](p3-04-multi-nav.md), [replan closure](p3-04-bounded-replan.md); `astrabot.nav.blocker_wait`, `reactive`, `replan`, `astrabot.fake_client` | Offline complete; live multi-Bot operation still open |
| P3-05 | Observed crouch hold/cross/headroom-safe release; supported Simple Jump with actual press receipt, landing and cooldown | [Crouch](p3-05-crouch-walk.md), [Jump closure](p3-05-jump-host.md); `astrabot.nav.crouch`, `simple_jump`, `jump_probe`, `jump_geometry`, `jump_walk`, `astrabot.fake_client` | Offline complete; standard-CS profile only |
| P3-06 | Measured map/fingerprint-bound ladder publication; owned up/down approach/contact/climb/exit/support, one-shot dismount and finite reacquisition | [Publication](p3-06-ladder-publication.md), [host closure](p3-06-ladder-host.md); `astrabot.adapter.ladder_scanner`, `astrabot.nav.enrichment`, `ladder`, `astrabot.fake_client` | Offline complete; supported geometry/profile only |
| P3-07 | Successful-dispatch progress history, oscillation detection, typed causes and finite wait/side/reverse/replan/abort without budget renewal | [Recovery closure](p3-07-progress-recovery.md); `astrabot.nav.recovery`, `walk`, `replan`, `astrabot.fake_client` | Offline complete |
| P3-08 | Independently declared finite matrix, ordered two-pass replay, malformed-evidence rejection, 1/8/16 actor isolation, map invalidation, portable CI and explicit pending live rows | [Gate](p3-08-offline-gate.md), [offline contract](../../tests/nav/simulation/README.md); `astrabot.nav.movement_checker`, `movement_replay`, `host_movement_replay` | Offline complete; hosted pass now observed |

Test names after a shared `astrabot.nav.` prefix are abbreviated in this table.
The two open main task checkboxes remain intentional: P3-01's full real-file
comparison and P3-08's post-Finish execution. Neither is silently marked done.
Drop, GapJump/LongJump, Boost, predictive crowds, combat/tactical planning and
learning remain outside this Phase 3 offline contract.

## Cross-task integration inspection

| Flow | Source connection and executable evidence | Result |
| --- | --- | --- |
| Operator to managed route | `NavConsole::execute` validates optional `slot:generation`, calls `requestRoute`, then `startMotion` only for an executable session; `route_session_tests.cpp`, registered-command fake-host tests | Connected; a Complete route is not arrival |
| Route to supported primitive | `corridor.cpp` owns selected transitions; `walk.cpp`, `walk_jump.cpp`, `walk_ladder.cpp` retain step/route identity and require observed support before advancement; corridor, Jump/ladder and host tests | Connected for supported transitions |
| Decision to actual dispatch | `LifecycleCoordinator::startFrame` guards and dispatches the previous queue, calls `afterDispatch`, then `moveFrame`; Motor and IntentPump keep per-frame transport separate from 25 Hz decisions | Connected; later-tick and one-command bounds tested |
| Feedback to finite recovery/replan | `motion.cpp` consumes actual transport receipts; actor-owned Recovery and ReplanAttempt outlive replacement Walk instances; P3-07 and host regressions | Connected; replacement routes cannot replenish retry allowance |
| Actor/map retirement | `serverDeactivate`, disconnect/removal and NavConsole invalidation clear pending state; P3-08 host matrix reuses the coordinator across map changes | Connected; stale dispatch rejected and fresh-map reuse observed offline |
| Producer to gate/CI | CMake registers Python checker and portable/host producers; `ci.yml` runs portable Windows/Linux and retains evidence with `if: always()` | Connected; hosted adapter/ELF runtime is not covered by these jobs |

No offline integration blocker was found in these inspected flows. This is a
contract/coverage audit, not a claim of exhaustive defect absence or engine
physics equivalence.

## Verification at the audited revision

The preceding main integration ran the full builds/tests. This audit inspected
their retained `LastTest.log` and movement context, then freshly ran the saved
evidence validator for Windows portable and adapter **before documentation
edits**. Both validators passed: **90 portable / 186 adapter rows**. Contexts
identify clean source `b9149c0` and x86; the Linux portable artifact also has
90 rows at that same clean revision. No old artifact was relabelled to the
later audit-documentation commit.

| Main build directory | Retained full CTest result | Replay executable SHA-256 |
| --- | --- | --- |
| `build-portable-x86-test` | 41/41, zero failures | `749b93d04adae86d18f471fa1158fb07532f7e6f9d01c01a4f8e16e92352bf2c` |
| `build-metamod-x86-test` | 47/47, zero failures | adapter: `8fef7ecd3d5980e66b09b3e7bd8c9d42593a1d76af539f91e576455174846e7f` |
| `build-linux-x86-p308` | 40/40, zero failures | `6ab72c47a4bf10a2ff4035f0a5501297a455df40a1b1a44fe750a9c63a115f03` |

These are Debug, warnings-as-errors, inspector ON, MSVC 19.51 / GCC 14.2 x86
gates. The Windows-only synthetic real-NAV-checker test explains 41 versus 40.
Portable 8/16/100 ms frame schedules and 1/8/16 actor matrices establish
synthetic determinism and isolation; specialized host traversal rows use one
actor. They do not establish live server capacity or multi-Bot ladder physics.
The prior P3-07 Release/six-export evidence remains historical; this audit
does not produce a newly pinned live DLL/SO artifact.

### Hosted CI follow-up

Read-only API and job-log inspection on 2026-09-06 confirmed
[run 34026542865](https://github.com/AoiKagase/AstraBot/actions/runs/34026542865),
push event, exact SHA `b9149c0560d365215d29680de94494fd90e21730`, completed
successfully at `2026-09-06T10:10:25Z`.

| Job | Job ID | Observed result | Artifact ID / SHA-256 digest |
| --- | --- | --- | --- |
| build-and-test | 101468406755 | Windows portable 41/41, movement checker/replay included | 9987255237 / `ce5fa8e64e501f56370dfc049c5da6cc2691cac97bce0dc2a345e156f2f29e76` |
| linux-x86 | 101468406749 | GCC `-m32` Debug, inspector ON, portable 40/40 | 9987248123 / `c8945cb75428270f881a1d89b65083f96a930eaf6c149f201cfe14a449520531` |
| nav-asan | 101468406638 | Dedicated ASan 6/6 and long-fuzz 1/1 | 9987265630 / `c15c44c37b6d1ca74b3682ead2ab4651c6f78bc8b3db3499ca987987709d70e1` |

Artifact names are `nav-debug-evidence`, `nav-linux-x86-evidence` and
`nav-asan-evidence`; all were reported unexpired. Digests/IDs are the GitHub
API metadata, not independently downloaded/archive-verified hashes. This
closes the P3-08 report's previously pending hosted run for **this source
revision**. It is not hosted Metamod testing or a pass for future code changes.

## Live readiness gaps and closure conditions

The [live manifest](../../tests/live/nav/manifest.json) has four environments,
11 scenario families, three FPS bands and three actor counts: **396 required
dimension keys, zero recorded results, `finishConfirmed: false`**. Each key
requires clean-map and mid-movement map-change observations, at least 792
variant observations before further door/jump/ladder success/failure subcases.
An empty result array is not a failed runtime test; it means no live acceptance
has been performed. The [checklist](../../tests/live/nav/README.md) defines the
record fields but is not yet an executable, fully specified runbook.

| Gap / affected contract | Current evidence | Closure required before the corresponding live row |
| --- | --- | --- |
| Project-wide Finish authorization | AGENTS.md requires all project phases/plans plus an explicit decision; the project specification still defines Phases 4–10 beyond movement | Assess and complete the remaining project scope, then record a separate explicit Finish decision. This Phase 3 audit cannot set it |
| Runtime and artifacts | Manifest runtime prerequisite is unresolved; Linux CI explicitly builds with Metamod OFF | Pin OS/arch, HLDS/ReHLDS, GameDLL, Metamod-P, plugin hashes and physics/cvars; identify tested x86 DLL and Linux SO with exports/dependencies/artifact hashes. Portable ELF is not the plugin |
| Lawful assets and independent oracles | Only named dust/dust2 v5 files have scoped comparison; per-row maps/starts/goals are absent | Supply lawful BSP/NAV with hashes, provenance and supported geometry; independently specify per-actor start/goal, expected arrival/failure and measurable thresholds before running |
| Multi-Bot operation | `lifecycle.cpp:164-178` exposes internal `createBot`; automatic map bootstrap creates only the primary actor (`:103-110`, `:199-209`). `console.cpp:98-104` registers only load/goto/status/cancel | Provide a reviewed operator/harness path to create/join/remove 8/16 managed actors, select teams/classes and record fresh identities. Internal fake-host API use does not supply a live console command |
| Complete evidence capture | `motion.cpp:51-62` stores a 128-record ring and prints rejection/cancel/terminal events; `console.cpp:253-257` prints the latest status, not a history drain. `nav_command.cpp` prints at most 64 route edges | Demonstrate a bounded collector that retains ordered decisions, primitive transitions, command buttons, actual dispatch receipts, query/time counters and terminal support with loss detection. Status polling alone cannot prove single Jump/Use edges or absence of stale commands |
| Row-level execution and result review | Manifest dimensions exist, but FPS targets, scenario subcases, numerical deadlines, per-actor outcomes and results are not populated; the Python checker validates offline producers only | Prepare concrete row sheets and a result-completeness review procedure (manual or validated tooling). Include up/down, use/touch/locked, success/finite failure, transient/permanent block, deliberate partial/unreachable input, cancellation and map-change triggers |

The internal actor seam and bounded debug history satisfy their stated offline
contracts. The gaps above concern how an operator can run and prove the full
live matrix; no new public API, exporter, server harness or production change
is implemented by this audit.

Real-NAV compatibility remains **Partially validated**, not universally blocked
by every other-version file: use each lawful, independently checked input only
within its established scope. Still open are generator/date/32-bit writer
provenance, sampled raw hiding/approach/encounter details and other real versions.
Unsupported versions or geometry cannot be waived to claim full compatibility.

## Operational preparation and ordering

Before Finish, keep portable CI running and prepare the missing runtime/asset
inventory, actor-control/capture design and independently expected row sheets.
Readiness changes that require code remain follow-up implementation under the
existing P3-04/P3-08 responsibilities; this audit creates no new task number.
The public command surface currently supports:

```text
astrabot_nav_load <matching-nav-path>
astrabot_goto <goal-area> [slot:generation]
astrabot_nav_status [slot:generation]
astrabot_nav_cancel [slot:generation]
```

Quote paths with spaces. NAV loading is map-wide; actor selectors are required
when managed actor selection is ambiguous. A `nav ... Complete` / `Ready`
line proves a route result only. Even a `walk state=Arrived` line needs retained
target-support and command evidence for the live acceptance record.

After an explicit project-wide Finish decision and closure of each relevant
readiness gap:

1. Record the Finish reference and exact runtime/artifact/configuration inputs.
2. Validate basic load, joined identity, safe motion, removal and map reuse for
   each environment; keep Vanilla CS and ReGameDLL observations distinct.
3. Run single-actor supported movement and finite-failure subcases with the
   declared FPS targets and measured frame-time distributions.
4. Run addressed 8/16-actor rows, verify unaffected actors continue, then cancel,
   remove/reuse slots and change map during active movement. Retain pre/post
   actor/map/link identity and show no old dispatch plus fresh-map reuse.
5. Append every attempt with its full dimension/subcase/variant key, UTC time,
   expected/observed results, counters, hashes and evidence paths. Keep failed
   attempts; mark unavailable inputs explicitly and identify the next action.
6. Reopen affected existing work on a failure. Declare live accepted only after
   every required row and subcase has an observed pass with complete evidence.

### Documentation drift

The Phase 3 plan's opening/next-session status and the current toolchain policy
are refreshed by this audit. The policy now matches AGENTS.md: all targets x86,
six exports including GiveFnptrsToDll, Linux portable CI before Finish.
Historical slice reports are not rewritten as new measurements.

Older Phase 1 live templates still contain obsolete x64/five-export/default
Visual Studio build instructions and the earlier Linux-before-Finish ban (for
example `tests/live/p1-04-fake-client.md` and `p1-06-movement.md`). They are
historical scenario references, not current build instructions. Use AGENTS.md
for current build/export policy and reconcile those templates when preparing
the executable live runbook. The Phase 1 plan's live-only acceptance wording
also needs explicit interpretation under the later project-wide Finish policy;
this audit does not declare Phase 1 live accepted or finish the whole project.

## Audit verification

Read the task criteria and closing reports, inspected the integration and
operator/capture paths, checked current main logs/provenance, freshly validated
both Windows replay artifacts, and inspected exact-SHA hosted jobs/logs/artifact
metadata. Documentation links, manifest dimension counts and whitespace are
checked before commit; FocalSpan is refreshed. Full build/test counts above are
the retained main/hosted runs at the audited source, not new executions caused
by this documentation-only change. No game assets or existing unrelated root
changes are modified, and no push is part of this audit.
