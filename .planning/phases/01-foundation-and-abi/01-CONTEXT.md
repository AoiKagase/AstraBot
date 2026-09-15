# Phase 1 Context: Foundation and ABI

## Goal

Create the smallest reproducible AstraBot project that can compile a portable SDK-free Core target and a minimal Metamod-P x86 plugin target without implementing Bot behavior.

## Locked decisions

- **D-01**: Runtime uses unmodified ReGameDLL-CS with Metamod-P, HLSDK, Engine, and public GameDLL boundaries only.
- **D-02**: Initial parity loads existing compatible `.nav` files read-only; Nav authoring belongs to the later AstraNav milestone.
- **D-03**: Windows x86 and Linux x86 are first-class targets; x64 configuration must fail early.
- **D-04**: Core must not include SDK/GameDLL types; adapter contracts use value types and explicit result values.
- **D-05**: ReGameDLL-CS and Metamod-P are behavior/SDK references only; their source trees are not copied or modified.
- **D-06**: Planning and implementation are sequential and use fine-grained, independently verifiable plans.

## In scope

- CMake project options and x86 compiler/build identity.
- C++14 project baseline aligned with the reference ReGameDLL build.
- Portable Core build/test target with no Metamod dependency.
- Metamod-P header consumption through an explicit external SDK path.
- Minimal C ABI/export contract and loadable plugin artifact.
- Source-origin manifest and artifact/architecture verification scripts.

## Out of scope

- FakeClient creation, hook behavior, native Bot suppression, commands, profiles, Nav parsing, movement, perception, combat, and game state.
- Any ReGameDLL-CS or Metamod-P source modification.
- ReAPI, private GameDLL symbol lookup, binary patching, Nav generation, or `astranav`.

## Source grounding

- ReGameDLL-CS reference: `H:/sourcecode/003.Game/amxmodx/ReGameDLL_CS` at `b0889847fe6d03898be88acc9e366660efb40ab5`.
- Metamod-P SDK: `H:/sourcecode/003.Game/amxmodx/metamod-p` at `7ec9b014f8c0a947a724644aebe34eb33706e44b`.
- ReGameDLL uses C++14 and enforces 32-bit Linux builds in its CMake configuration.
- Metamod-P exposes x86 Linux/Win32 build settings and public SDK headers under `hlsdk` and `metamod`.
- Architecture and non-copy boundary: `docs/superpowers/specs/2026-09-15-astrabot-csbot-clone-design.md`.

## Completion evidence

- Windows x86 Debug portable configure/build/test succeeds.
- Linux x86 Debug portable configure/build/test succeeds when the multilib toolchain is available.
- Windows/Linux x86 Metamod artifact compiles against the pinned SDK.
- Required export names and x86 architecture are verified.
- Source manifest checker passes and records every Phase 1 project source file.
