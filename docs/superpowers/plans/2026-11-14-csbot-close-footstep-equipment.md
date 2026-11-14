# Close Footstep Equipment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline.

**Goal:** Equip a usable pistol when a sniper hears a close enemy footstep, matching pinned CSBot.
**Architecture:** SDK-free receipt/classifier and existing equipment planner; observation publishes a typed request before noise memory replacement. Existing action/command transport owns dispatch.
**Tech Stack:** C++14, public HLSDK/Metamod-P, Windows/Linux x86.
**Spec:** docs/superpowers/specs/2026-11-14-csbot-close-footstep-equipment-design.md

## Global Constraints
Read-only pinned reference; independent implementation, seven exports unchanged. Preserve SDK-free Core and unrelated working edits/profile/fixtures. No private offsets/NAV edits. Serialize OS CTests and use VS HostX86/x86 for complete Windows build.

## Review Focus
- Raw distance before NAV error and memory replacement; hostile source only.
- Exactly300, loaded sniper, unknown/empty pistol ammo and bot_allow_pistols.
- Repeated sound/command cadence/CurWeapon confirmation use shared5s interval.
- USE/C4/grenade/freeze/disabled/death/round/clock/actor lifecycle cannot leak stale requests.
- Actual command sent without an enemy Aim/fire or extra per-frame scans.

### Task1: Pure policy
Files: new compat/footstep_equipment.hpp; modify weapon_equipment.hpp and tests/weapon_equipment_tests.cpp, source-manifest.json.
Interfaces: FootstepEquipmentReceipt (actor/frame/seconds/distance), isStandardFootstepSample(), EquipmentInput::closeFootstepDistance.
- [x] Write pure whitelist/strictdistance/sniper/clip/skill/ammo/cooldown assertions.
- [x] Run focused test; Expected: RED before implementation.
- [x] Implement bounded receipt and close-footstep equipment branch; run focused suite Expected: PASS.

### Task2: Public event and runtime
Files: observation recordSound/onEmitSound, behavior equipment/runtime resets, services/plugin wrapper, ownership manifest, weapon_switch_runtime_tests.cpp.
Interface: behavior_observeCloseFootstep(index,const FootstepEquipmentReceipt&) only stores data; decideEquipmentAction consumes once via normal action pipeline.
- [x] Runtime regression from public sounds to actual pistol command; Expected: RED before wiring.
- [x] Wire validated hostile raw receipts, standard material steps, current observed sniper and existing command transport.
- [x] Focused runtime/unchanged characterization Expected: PASS.

### Task3: Review and delivery
- [x] Independent read-only final review; Important/Critical fixes RED→GREEN.
- [x] Complete both x86 builds; sequential CTests; PE/ELF7exports, manifest/ownership/ports/whitespace gates. Expected: Windows all PASS; Linux only known combat_policy failure.
- [x] Record source/artifact/profile identity and scope in STATUS; explicit-path commit and local FF merge; no push/deployment.

## Ledger
Ruling: continuing user instruction authorizes execution and established local commit/merge workflow without another design/merge approval. Current checkout branch and existing caches preserved.

Task1: complete — missing-header RED→GREEN; pure policy PASS.
Task2: complete — actual missing sniper command RED→GREEN; runtime guards and stronger memory PASS.
Task3: complete — two Important runtime RED→GREEN, full Windows102/102 and Linux100/101 (only existing combat_policy), frozen baseline and independent export/ownership/source gates PASS. Explicit commit/local FF is enclosing delivery.
Final: Ruling: public EmitSound coverage only; PM_Move/native animation intake remains separate. Cost: some native footsteps absent.
Final: Ruling:0.25s next-decision window retains existing command scheduling and drops later requests. Cost: slow decisions miss the equip opportunity. No deferred minors.
