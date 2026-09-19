# P00 Reference Freeze

## Scope

This document freezes the behavioral comparison target for P00. ReGameDLL-CS is
an observation/reference oracle only; no ReGameDLL implementation code is copied
into AstraBot.

| Item | Frozen value |
|---|---|
| Repository | `https://github.com/rehlds/ReGameDLL_CS.git` |
| Branch | `master` |
| Commit | `b0889847fe6d03898be88acc9e366660efb40ab5` |
| Commit subject | `Add mp_longjump_cooldown CVar to prevent LongJump abuse (#1181)` |
| Commit date | `2026-08-28T02:19:49+03:00` |
| Local observation time | `2026-09-19T10:18:30+09:00` (Tokyo Standard Time) |
| Reference source roots | `regamedll/game_shared/bot/`, `regamedll/dlls/bot/`, `regamedll/dlls/bot/states/` |

The reference checkout was not clean when inspected. Its uncommitted state
included `.gitignore`, MSVC project files, a deleted `extra/HostageImprov`
archive, and an untracked `.focalspan.json`. Those changes are excluded from
this freeze; all source claims in P00 use the commit above.

## Reference build semantics

The pinned `regamedll/CMakeLists.txt` defines the following relevant defaults and
definitions:

- C++14 is required.
- `DEBUG`, `USE_STATIC_LIBSTDC`, `USE_LEGACY_LIBC`, and `XASH_COMPAT` are off
  for the ordinary reference build unless an external build explicitly changes
  them.
- On a 64-bit host without Xash compatibility, the build forces 32-bit output
  (`-m32` and a 4-byte pointer model).
- The target definitions include `REGAMEDLL_FIXES`, `REGAMEDLL_API`,
  `REGAMEDLL_ADD`, `UNICODE_FIXES`, `BUILD_LATEST`, `CLIENT_WEAPONS`,
  `USE_QSTRING`, `_LINUX`, `LINUX`, `NDEBUG`, and
  `_GLIBCXX_USE_CXX11_ABI=0`, plus the documented libc name mappings.
- The CSBot sources are compiled into the GameDLL target, including the common
  bot library, `cs_bot*`, `cs_gamestate*`, and every file in `dlls/bot/states`.

The exact reference binary was not rebuilt during P00. These are the pinned
source/build semantics, not a claim that a new reference binary was produced.

## Intended live stack

Existing repository evidence, recorded separately from this P00 audit, identifies
the Windows x86 stack as:

| Component | Evidence value |
|---|---|
| HLDS | `48/1.1.2.7/Stdio/9909` |
| ReGameDLL-CS | `5.30.0.830-dev+m`, source `b0889847fe6d03898be88acc9e366660efb40ab5` |
| Metamod-P | `1.21p109`, source `7ec9b014f8c0a947a724644aebe34eb33706e44b` |
| Map / NAV | `de_dust2.bsp` / read-only `de_dust2.nav` |
| Target | 32-bit Windows x86 |

The existing Windows evidence is partial: join/team and human damage/death were
observed, but autonomous movement, Bot-to-Bot combat, and C4 events were not.
The Debian Linux x86 live target remains pending. See
`docs/evidence/phase8-live-acceptance.md`; this is not promoted to a P00 parity
pass.

## AstraBot baseline

| Item | Value |
|---|---|
| Commit at P00 start | `9788c245f5017567c60c35fa3afe474d56d1a349` |
| Commit subject | `Implement Phase 8 Nav movement and C4 action boundaries` |
| Working tree | Dirty before P00; unrelated/user changes were preserved |
| Source inventory | 80 files under `src/` and `include/`; 61 files under `tests/`/fixtures |
| Build directory | `build-action-adapter-x86-1451` |
| Generator | `NMake Makefiles` |
| Compiler | VS 2026 MSVC 14.51.36231 `Hostx86\x86\cl.exe` |
| Build type | `Debug` |
| Astra options | `ASTRABOT_BUILD_METAMOD=ON`, `ASTRABOT_BUILD_TESTS=ON`, `ASTRABOT_WARNINGS_AS_ERRORS=ON` |
| Fresh built DLL SHA-256 | `3d2bda1bd90065ca5553d0b4de13e9d6e32f03c59f099981c705d5e81dcc4d0f` |

The AstraBot commit and SHA identify the audit baseline only. The existing
working-tree changes were not staged or committed by P00.
