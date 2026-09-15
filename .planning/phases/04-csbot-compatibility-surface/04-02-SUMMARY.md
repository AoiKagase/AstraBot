---
phase: 4
plan: 02
subsystem: csbot-profile-compatibility
tags: [profiles, parser, bounded-input, transactional-load, x86]
requires:
  - plan: 04-01
provides:
  - fixed-capacity format-neutral ProfileCatalog
  - bounded read-only BotProfile.db-compatible loader
  - deterministic bot_add profile selection boundary
affects:
  - 04-03 actor command execution
actuals:
  tasks: 4
  commits: 0
  verification: passed
requirements-completed: [COMP-02, TEST-02]
---

# Plan 04-02 Summary

Added a clean-room, SDK-free profile catalog and a Metamod adapter loader.
Profile records use fixed-size storage for names and bounded numeric fields.
The loader accepts block-based BotProfile input, supports quoted names,
team/difficulty/skill/aggression fields, derives a difficulty bucket from
skill when needed, and ignores unrelated scalar profile fields without
depending on ReGameDLL-CS implementation classes.

Loading is read-only and transactional: bytes are read into a bounded buffer,
the complete candidate catalog is validated, and only then replaces the
published catalog. Duplicate names, malformed input, file-size limits,
profile-count limits, missing files, and invalid arguments return explicit
results and preserve the previous catalog.

## Files

- `include/astrabot/compat/profile_catalog.hpp`
- `src/core/compat/profile_catalog.cpp`
- `include/astrabot/metamod/profile_loader.hpp`
- `src/adapter/metamod/profile_loader.cpp`
- `tests/profile_catalog_tests.cpp`
- `tests/profile_loader_tests.cpp`
- `include/astrabot/metamod/compat_surface.hpp`
- `src/adapter/metamod/compat_surface.cpp`
- `docs/source-manifest.json`

## Verification

- RED observed before implementation: `profile_catalog.hpp` was absent.
- Windows x86 portable profile tests: 2/2 passed.
- Windows x86 Metamod CTest: 15/15 passed.
- Debian WSL Linux x86 portable profile tests: 2/2 passed.
- Debian WSL Linux x86 Metamod CTest: 15/15 passed.
- Source manifest: 50 entries, 45 C/C++ files.
- PE and ELF artifact checks: x86 with six exact exports each.
- C/C++ source format: 45 files, LF, UTF-8 without BOM, no trailing
  whitespace, final newline.
- FocalSpan and CRG indexes refreshed against current staged source.

## Scope boundary

The loader does not create actors or write profile/Nav data. Existing profile
files remain input-only; actor execution is deferred to 04-03 and real server
acceptance remains a separate evidence layer.
