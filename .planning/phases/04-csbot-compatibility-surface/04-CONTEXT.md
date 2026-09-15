---
phase: 04-csbot-compatibility-surface
status: approved
source: docs/superpowers/specs/2026-09-15-astrabot-csbot-clone-design.md
---

# Phase 4 Context: CSBot Compatibility Surface

## Goal

Existing Counter-Strike server configuration can address AstraBot through the
required `bot_*` commands, CVars, difficulty/team settings, and profile names
without loading or copying ReGameDLL-CS bot implementation.

## Locked decisions

- ReGameDLL-CS remains behavior-only reference input; no private symbols,
  headers, profile manager, or bot source are linked or copied.
- Public Metamod/GoldSrc engine functions and AstraBot-owned Core contracts
  are the only runtime boundaries.
- Phase 2 native command blocking must distinguish native registration from an
  AstraBot-owned registration. The guard remains fail-closed for unknown
  ownership.
- Commands execute on the main thread and return explicit handled/rejected
  results. Invalid arguments must not mutate actor or quota state.
- Profile data is parsed into a format-neutral value catalog. The parser is
  bounded and read-only; `BotProfile.db` is input, not an implementation
  dependency.
- Full bot behavior, Nav, locomotion, learning, and AstraNav remain later
  phases.

## Public reference evidence

The pinned ReGameDLL-CS reference registers public bot CVars, registers native
server commands through the engine callback, and routes `bot_add*` through a
profile/difficulty/team selection path. Metamod-P exposes public CVar
register/get/set functions and `pfnAddServerCommand`; AstraBot must preserve
the six-export ABI while using these hooks internally.

## Verification boundary

- Core tests cover command parsing, argument validation, CVar desired-state
  transitions, profile selection, duplicate/unknown handling, and bounded
  catalogs.
- Adapter tests use fake engine callbacks and prove AstraBot-owned commands
  are registered/handled while native registrations remain blocked.
- Windows/Linux x86 offline checks are required. Live server configuration
  migration remains a later acceptance layer.
