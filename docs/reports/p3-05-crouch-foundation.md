# P3-05 traversal constraints and crouch posture foundation

This separately testable boundary starts from `d2461d1` on
`codex/p305-crouch`. It is not a completed P3-05 implementation slice.

The manifest-pinned ReGameDLL_CS revision
`b0889847fe6d03898be88acc9e366660efb40ab5`,
`regamedll/game_shared/bot/nav.h`, was read from the original Phase 0 temporary
checkout. It defines Crouch=0x01, Jump=0x02, Precise=0x04 and NoJump=0x08.
The source was used as format evidence; no movement implementation was copied.

The pure constraints function combines source/target hints and the selected
traversal. It distinguishes Walk, Crouch and Jump; preserves NoJump; rejects
Jump+NoJump, unsupported duck-jump, Precise, unknown bits and unsupported
traversal tags with typed reasons. Parsing and stored NAV attributes stay intact.
A Jump result is a requirement for a future verified controller, never a button.

Crouch is a route-bound posture gate with Standing, Lowering, Crouched, Raising,
Failed and Aborted states. The owner supplies standing/crouched hull dimensions
and a finite transition timeout. Permission requires actual grounded, duck-flag
and hull observations to agree. An intermediate/mismatched hull cannot authorize
movement. Requests to change posture use one stamped stationary clearance query,
preserving the observed foot position while changing the hull center/height.
Release waits for fresh standing clearance and then observed standing posture.
Low ceilings retain duck, zero translation and a finite timeout. Query failures,
identity changes, repeated ticks, stopped/backwards clocks and exhausted query
budgets cannot authorize movement. Abort while ducked holds duck safely; the
event is emitted once. A lifecycle owner must discard commands for invalid actors.

Tests cover hints/conflicts, 8/16/100ms timing, actual hull confirmation, low
ceiling/release, ordinal reservation, stale/failed/missing/throwing queries,
actor generation change, timeout, overflow-safe duration, abort and terminal
replay. The motor check confirms release has no duck button or translation.

This gate does not authorize a path or claim a portal crossing/arrival. The
next implementation must integrate it with Walk's measured floor/hull/portal
checks, reserve its query in the shared 21-query budget, hold duck on cached
commands, and retain safe stop/release behavior through host cancellation.
Existing Walk still rejects special hints; no crouch command is exposed yet.
Simple Jump approach/align/accelerate/takeoff/airborne/landing/cooldown follows.
Both P3-05 checklists remain open. No live server or Finish decision is included.

Verification: Windows x86 NMake Debug adapter/portable 36/36 PASS; WSL Debian
GCC -m32 Debug portable 31/31 PASS; warnings-as-errors enabled. Windows x86
Release DLL builds with the six contracted exports. FocalSpan updated and diff
reviewed before commit. Hosted CI remains pending; nothing was pushed.
