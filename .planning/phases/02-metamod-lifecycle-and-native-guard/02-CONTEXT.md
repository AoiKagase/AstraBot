---
phase: 02-metamod-lifecycle-and-native-guard
status: approved
source: docs/superpowers/specs/2026-09-15-astrabot-csbot-clone-design.md
---

# Phase 2 Context: Metamod Lifecycle and Native Guard

## Goal

The plugin must load into an unmodified ReGameDLL-CS server, receive the
public Metamod-P lifecycle hooks, own its map/session state, and prevent
silent coexistence with the native ReGameDLL-CS CSBot manager.

## Locked decisions

- ReGameDLL-CS and Metamod-P remain read-only reference inputs.
- No private ReGameDLL symbol, ReAPI dependency, DLL patch, or copied SDK
  source is allowed.
- SDK and Metamod types stay in `src/adapter/metamod`; SDK-free generation
  state stays in `astrabot_core`.
- The plugin targets C++14 and 32-bit x86 on Windows and Linux.
- Engine and GameDLL callbacks are main-thread-only and must not throw across
  the C ABI boundary.
- Phase 2 does not create AstraBot FakeClients and does not implement the
  `bot_*` compatibility surface. Those are Phase 3 and Phase 4 concerns.
- Native guard behavior is fail-closed: if the public boundary cannot prove
  that native ownership is disabled, later managed-Bot creation remains
  disallowed and a diagnostic is emitted.
- Existing `.nav` files are not loaded or written in this phase.
- Round generation is advanced only from an explicit public round-boundary
  signal or a conservative, tested frame-reset observation. The adapter must
  not infer a round from private GameDLL state.

## Public boundary evidence

The pinned Metamod-P headers expose `META_FUNCTIONS` callbacks for
`GetEntityAPI2` and `GetEngineFunctions`. The `DLL_FUNCTIONS` table includes
`ClientDisconnect`, `ClientPutInServer`, `ServerActivate`,
`ServerDeactivate`, and `StartFrame`. The engine table exposes the CVar and
entity inspection functions needed by the guard, including
`pfnCVarGetPointer`, `pfnCVarSetFloat`, `pfnAddServerCommand`,
`pfnIndexOfEdict`, and `pfnPEntityOfEntIndex`.

The reference manager registers native bot server commands and uses the
public `bot_enable` and `bot_quota` CVars. AstraBot may observe and constrain
those public controls, but it must not call `TheBots`, `CCSBotManager`, or
any other private GameDLL object.

FocalSpan verification against the pinned dependency checkouts confirmed the
boundary used by this phase: ReGameDLL-CS calls its native manager from
`ServerActivate` and `StartFrame`, and its manager monitors the public bot
CVars. Its `ServerDeactivate` path is intentionally idempotent. Metamod-P's
public `GET_HOOK_TABLES` utility exposes the hooked engine/DLL tables, while
`RETURN_META(MRES_SUPERCEDE)` is the supported way for an engine pre-hook to
stop a native registration. These observations are behavioral/ABI evidence;
no dependency source is copied into AstraBot.

## Verification boundary

- Unit and adapter tests prove callback table population, invalid-input
  rejection, lifecycle ordering, generation invalidation, CVar guard
  decisions, and unmanaged FakeClient detection.
- Windows x86 and Linux x86 build/test checks are required when their
  toolchains are available.
- This phase does not claim live HLDS/ReHLDS gameplay parity. Live plugin
  loading and native-spawn race acceptance remain explicit later evidence.
