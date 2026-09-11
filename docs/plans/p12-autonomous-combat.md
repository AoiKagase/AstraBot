# P12: autonomous BOT-to-BOT combat

User-approved implementation scope, 2026-09-10. This refines P12-03/04/05/06; it does not create a new phase or declare project-wide Finish.

## Acceptance

Every managed, joined, living BOT must receive actor-specific input, perception, tactical, movement and combat updates. Without per-BOT goto commands it must roam, engage visible enemy BOTs, and resume after failure, loss of target, reload, death and subsequent round spawn. A non-primary BOT that never progresses fails acceptance. Command submission alone does not establish movement or combat.

Use 1v1, then 2v2, then the configured operational BOT count for at least ten minutes and three rounds. Preserve per-actor evidence of movement and combat participation, plus actual damage/death and next-round recovery. Kills are not required for every BOT. Check route failures, Jump/Drop, loss of vision, ammunition depletion, add/remove, map changes and edict reuse. Objective strategy, economy, learning and Linux acceptance remain separately owned.

## Implementation contracts

- Separate route search success from motion execution. Report corridor and primitive failures to the planner, retire stale pending input, and never return Unchanged for failed motion solely because the search is Ready.
- Compose bounded structural directed-edge exclusions with existing route costs. Failed goals cool down for two seconds; new searches back off 250ms. Transient observations/blockers must not permanently poison topology. Exclusion-capacity exhaustion is explicit and fail-closed.
- Preserve actor/generation isolation and clear retired lifecycle state. Idle is a neutral transport heartbeat, not an AI intent and not intrinsically a failure.
- Derive Walk/Jump/Drop from NAV geometry and supported attributes. NAV tile boundaries are not automatically walls. Physical hull/support checks remain mandatory.
- Reuse Jump physics/dispatch/landing machinery for attributed micro areas. Drop uses approach, step-off, airborne and observed landing, bounded initially to 128 units vertical and 32 units horizontal. No direct velocity writes, guessed landing or unconditional Walk fallback.
- Reuse perception/combat with actual team/alive/weapon/ammo/cooldown and LOS authorization. Preserve combat-only submission without a moving NAV command. Do not invent unseen enemies or objectives.
- Distinguish selected edge from failing transition edge. Record actor/tick-stamped execution, target/fire decisions and observed damage/death with bounded diagnostics.
- On initial ServerActivate and changelevel, retire the previous map's routes/commands and automatically load `<game-directory>/maps/<current-map>.nav` once per new map generation, before BOT creation. Missing/invalid input is fail-closed and logged as `nav_auto`; explicit `astrabot_nav_load` remains the recovery path. Do not retry each StartFrame or rely on a primary BOT.

## Verification boundary

Regression sources are added/reviewed before live PASS; CTest configurations/builds, CTest and canonical execution remain prohibited until explicit user-confirmed live PASS. Source review, diff checks, FocalSpan and tests-OFF Release compilation are not live acceptance. Server operations and deployment require their applicable authorization. Preserve unrelated worktree inputs and commit only intended source/tests/docs.

## Current result

Source implementation and integration are complete; runtime acceptance remains pending. Added regression sources cover NAV execution failure/cooldown/actor isolation, map lifecycle autoload, derived Drop geometry and support rejection, micro Jump, and runtime actor retirement/diagnostics. These tests are not yet compiled or executed under the pre-live gate.

2026-09-10 verification:

- Configured and built only `astrabot_mm` in `build-metamod-x86-release`, NMake Makefiles, Release, x86, tests OFF, warnings-as-errors ON. Visual Studio 2026 Community `VsDevCmd.bat -arch=x86 -host_arch=x64`; pinned Metamod-P SDK `7ec9b014f8c0a947a724644aebe34eb33706e44b`, clean SDK working tree. Source baseline before changes: `c16d431`.
- Initial compile caught Windows SDK `near` macro collision in the Drop guard and `snprintf` header ordering in lifecycle. Fixed those source issues and rebuilt only the affected DLL target; final build exited 0.
- `dumpbin /exports` confirms exactly six undecorated exports: Meta_Query, Meta_Attach, Meta_Detach, GetEntityAPI2, GetEngineFunctions, GiveFnptrsToDll.
- Built DLL SHA-256: `FB2FDAF210DD2634231FFE51DAABD839C4FAC631B8323936D741E801AD0EDB13` (18:27 JST), 741376 bytes. Includes final Motor/transport-rejection retirement. Artifact remains in the workspace, not deployed.
- `git diff --check` passes. Final FocalSpan refresh and narrow commit are recorded in the task handoff.
- No Debug/test build, CTest, canonical verification, server operation or live acceptance was performed. Observed health-loss diagnostics do not identify an attacker and do not prove BOT-to-BOT damage without matching live events.
