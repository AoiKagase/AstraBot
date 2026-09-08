# P12 runtime input integration

Baseline: `a811135`. Review input: `docs/astrabot-rereview-a811135.md`.

## Findings and decisions

- HIGH-01: replace the absent production provider with an Adapter-owned,
  synchronous self-state reader. The pinned HLSDK callback surface supplies
  weapon ID, inventory, clip, reserve ammo, reload, and relative cooldowns.
  The local ReGameDLL `dlls/client.cpp` GetWeaponData/UpdateClientData definitions
  establish the CS wire mapping (`vuser4.y` reserve ammo, `iuser3` shoot/freeze bits).
  No private-data offsets or ReGameDLL headers are added to Core or the build.
- Read PlayerRegistry/agent binding, joined primary, current entity, affiliation,
  WorldModel and current-map NAV together. Recheck identity after each GameDLL
  callback. Invalid input retires prior runtime-owned movement and combat.
- Objective/economy observations have explicit DTOs. Their real readers remain
  unavailable: team strategy is neutral, tactical/action objectives are None,
  and economy is unavailable. Unknown objective state is not marked known.
  Full bomb/hostage/VIP/escape/economy behavior remains unimplemented here.
- NAV area lookup uses current position, not an old route trace. A current
  executable route supplies a Hold route target and geometric lower-bound ETA.
  Without a route, tactical planning holds the current area; this is not roaming
  or autonomous objective selection. Stationary combat reaches command
  composition and the existing next-tick engine transport.
- Pre-dispatch self/weapon validation retires a queued runtime command on invalid
  observation, changed active weapon, death, missing NAV, or stale identity.
  An attack additionally requires ammunition, no reload and current cooldown.
- TeamDirector's `teamExecuted` diagnostic now reflects actual updates.
- Normalize GoldSrc yaw (including 270 degrees) to the Core angle range.
- LOW-02 was already fixed: enabled all-zero BSP/NAV hashes are rejected by
  `MapIdentity::valid`; no duplicate implementation is added. Existing traffic
  weighting and atomic replace/backup semantics remain unchanged.
- Strict Metamod compatibility, live NAV, performance and stability acceptance
  remain post-Finish. No live server is started, and Finish is not declared.

## Files

`runtime_input.hpp/.cpp`, lifecycle, NavConsole, RuntimeOrchestrator,
adapter host tests, test registration and this Phase 12 evidence.

## Verification

Focused verification uses the existing x86 Debug Metamod profile and only
runtime_input, runtime_orchestrator, entry and fake_client tests. The final
canonical All gate runs after these checks and implementation are complete.
An initial gate encountered a replay context change caused by staging during
the run; the exact failed test passed after Git state was fixed. A subsequent
inspection found the yaw normalization gap above, so that earlier content is
not the final verification identity. The final gate is for the corrected content.
Its generated verification record is the authority for the content fingerprint,
toolchain, SDK, HEAD and tree. Commit/fast-forward alone do not repeat the gate.

Final canonical All completed successfully on 2026-09-08:

- Portable x86 Debug: 65/65.
- Metamod x86 Debug: 84/84.
- Metamod x86 Release: six required undecorated exports.

Final input fingerprints (unchanged by this documentation-only result entry):

| Profile | Fingerprint |
| --- | --- |
| PortableDebug | `b731b67e003a3ed7e4c8395e532dae4f4fa1920d892d8d3c4fb93ebb2398cef3` |
| MetamodDebug | `cd172aeb949af098526329511d03b779e261264eedfabb8e8fff5edba189089f` |
| MetamodRelease | `612c9cfff7ad36a6416bf618b32e417026cbf8450af202345bffb94f8be81714` |
