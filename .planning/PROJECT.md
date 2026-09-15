# AstraBot

## What This Is

AstraBot is an independently implemented Counter-Strike 1.6 Bot delivered as a Metamod-P plugin. The first milestone reproduces the observable CSBot/ZBot runtime behavior and existing server operation against an unmodified ReGameDLL-CS, using existing compatible `.nav` files as read-only input.

Nav generation, learning, editing, analysis, persistence, and the optimized `astranav` format are deliberately deferred to a later AstraNav milestone.

## Core Value

An administrator can replace the ReGameDLL-CS CSBot runtime with a standalone AstraBot Metamod plugin without changing normal Bot commands, profiles, Nav inputs, or gameplay expectations.

## Requirements

### Validated

None yet — ship to validate.

### Active

- [ ] AstraBot loads as a standalone Metamod-P plugin beside unmodified ReGameDLL-CS.
- [ ] Existing `bot_*` operation, profiles, server configuration, and compatible `.nav` inputs remain usable.
- [ ] CSBot/ZBot lifecycle, navigation execution, perception, combat, objectives, state transitions, communication, and round recovery are reproduced closely enough for replacement.
- [ ] Windows 32-bit and Linux 32-bit builds and live acceptance are maintained separately from offline Core verification.
- [ ] Core remains independent from HLSDK/GameDLL types and future AstraNav formats.

### Out of Scope

- Nav generation or automatic map learning in the initial CSBot parity milestone — owned by the later AstraNav milestone.
- Nav editing, analysis-to-file, `.nav` write-back, and `astranav` generation in the initial milestone — avoids mixing replacement parity with a new Nav authoring system.
- Wallbang, adaptive learned routing, persistent experience, and advanced human/Bot traversal learning in the initial milestone — these are AstraBot extensions after parity.
- ReAPI, private ReGameDLL symbols, ReGameDLL patching, and relinking — the runtime boundary is Metamod-P plus public HLSDK/Engine/GameDLL interfaces.

## Context

- Reference ReGameDLL-CS commit: `b0889847fe6d03898be88acc9e366660efb40ab5`.
- Reference Metamod-P SDK commit: `7ec9b014f8c0a947a724644aebe34eb33706e44b`.
- The ReGameDLL-CS checkout contains unrelated local modifications and is read-only reference input.
- ReGameDLL-CS CSBot is a large subsystem covering management, FakeClient lifecycle, Nav/pathfinding, perception, combat/weapons, objectives, state machines, radio/chatter, and map learning.
- The design is documented in `docs/superpowers/specs/2026-09-15-astrabot-csbot-clone-design.md`.
- `AstraBot_bk` is a behavioral/design comparator only. Its source is not copied into this project.

## Constraints

- **Runtime boundary**: Use unmodified ReGameDLL-CS with Metamod-P, HLSDK, Engine, and public GameDLL boundaries only — preserves the standalone plugin requirement.
- **Platforms**: Support Windows x86 and Linux x86 — matches the intended GoldSrc/Metamod-P deployment targets.
- **Nav input**: Initial runtime loads existing compatible `.nav` files read-only — Nav authoring belongs to AstraNav.
- **Architecture**: Keep Core SDK-free and translate through GoldSrc/Metamod adapters — enables deterministic tests and future format changes.
- **Compatibility**: Preserve existing `bot_*` commands/CVars, profiles, server configuration, and `.nav` input behavior — permits operational replacement.
- **Safety**: Fail closed on ABI mismatch, invalid Nav, stale Entity generations, invalid Trace, command rejection, or native Bot mixing — prevents crashes and omniscient behavior.
- **Code style**: Apply `C_CPP_REFACTOR_RULES.md` to all AstraBot C/C++ code and tests — keeps generated implementation consistent.
- **Evidence**: Use code-review-graph and FocalSpan as directed by `AGENTS.md`; distinguish offline verification from live acceptance.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Independent Core plus GoldSrc/Metamod adapters | Separates Bot logic from SDK and enables deterministic verification | — Pending |
| Standard Metamod-P boundary only | Prevents coupling to ReGameDLL private implementation and keeps the plugin replaceable | — Pending |
| CSBot parity before AstraNav | Establishes a working replacement before introducing new Nav authoring behavior | — Pending |
| Existing `.nav` read-only in v1 | Avoids conflating legacy compatibility with the future `astranav` design | — Pending |
| Windows/Linux x86 as release targets | Covers the intended GoldSrc server environments | — Pending |

## Evolution

This document evolves with phase transitions and milestone boundaries.

After each phase transition:

1. Move invalidated requirements to Out of Scope with a reason.
2. Move verified requirements to Validated with a phase reference.
3. Add newly discovered requirements to Active.
4. Record decisions and outcomes in the Key Decisions table.
5. Update the project description if the product boundary changes.

After each milestone:

1. Review all sections and the Core Value.
2. Audit Out of Scope reasons to prevent accidental scope creep.
3. Update Context with current evidence and acceptance state.

---
*Last updated: 2026-09-15 after GSD initialization*
