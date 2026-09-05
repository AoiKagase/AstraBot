# P3-04 per-player host seam

This is the separately reviewable adapter seam built from `1a3d8d7`. No live
server, deployment or project-wide Finish was performed.

LifecycleCoordinator owns at most 32 client states. Each owns the existing
fake-client factory, join state, decoder and deferred join-cleanup state.
`createBot`, player-specific `requestJoin`, `remove`, `entityFor` and
`joinState` access these records. The old primary convenience methods and
automatic primary bootstrap remain for existing callers. Portable registries
and command values retain their existing contracts.

Each published client captures its PlayerId, map and edict serial. Resolution
checks current registry generation, map, free state, serial, engine index and
the reverse entity lookup when available. The player observer now obtains
priority identity through this managed resolver; a registry slot alone cannot
establish an entity binding. Unmapped observed players remain Other.

Command ingress checks the addressed client's join and resolved entity.
Dispatch consumes only that player's pending slot; a failed or unjoined actor
does not drain another queue. Disconnect/removal clears only the matching
client's pending movement and join/decoder state. Stale serials cannot cause a
kick or direct cleanup of a reused entity. An occupied slot returned by creation
is rejected without deleting the existing player. Removal intent is published
before synchronous ServerExecute can acknowledge disconnect.

Per-client decoders preserve interleaved ShowMenu fragments. Broadcast messages
capture map and registry generations at begin, so a TeamInfo that spans slot
reuse cannot update the replacement join. Command argument context remains
synchronous and rejects nested dispatch. All client states are invalidated
before detach calls any GameDLL disconnect callback.

Synthetic adapter tests create two distinct fake clients with different teams
and join progress. They verify addressed menu commands, independent movement
values/entities, a queued command rejected by actor death/serial reuse, duplicate
allocation without cross-actor removal, replacement generation, stale TeamInfo,
disconnect, map invalidation and two-client detach cleanup. A transport test
separately proves an unjoined actor's rejected dispatch preserves the other
actor's queue. Existing single-client and NAV integration tests remain active.

Windows x86 NMake Debug adapter/portable tests pass 34/34 with warnings-as-errors;
Release x86 builds and the six contracted exports pass. WSL Debian Linux x86
portable tests pass 29/29. Hosted CI remains pending.

Still open: NavConsole currently has one route/motion owner and requires a
unique managed actor. Explicit per-actor navigation selection/sessions and
automatic bounded replan consumption are the next P3-04 work. These command
tests are not two-Bot route-following or live-server acceptance. P3-04 stays open,
as do the remaining Phase 3 plans; real NAV compatibility remains partial.
