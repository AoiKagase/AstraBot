# Phase 9 Offline Gate

## Scope

This gate covers the engine-independent Persistent Experience layer. The
navigation graph remains immutable, engine events are converted to value-level
`ExperienceEvent` records, and persistence is reached only through
`IExperiencePersistence` / `ExperiencePipeline`.

## Evidence

| Area | Evidence |
| --- | --- |
| Event contracts | `src/core/experience.hpp` defines AreaEntered, combat/outcome, objective, and human/Bot traversal events with bounded value validation. |
| Area model | `AreaExperience` stores visits, team danger, encounter/death, grenade/sniper threat, push/retake success, traffic, and outcome metrics. |
| Human/Bot separation | Human and Bot traffic/visit counters remain separate; `ExperienceSettings` keeps configurable weights, defaulting to human `1.0` and Bot `0.25`. |
| Map safety | Every event and snapshot carries a full map identity: name, BSP size/hash, NAV format, and NAV hash. Mismatches are rejected and persisted files are quarantined without overwriting an existing quarantine. |
| Decay | Round-boundary decay applies a fixed `9/10` factor per elapsed round, bounded to 256 steps, with deterministic ordering and no wall-clock reads. |
| Persistence | The versioned little-endian binary store has checksummed payloads, schema/version checks, schema-1 migration, temp-file replacement, one-generation backup, corruption recovery, and map mismatch quarantine. |
| Pipeline boundary | `ExperiencePipeline::submit` updates the model; `restore` and `flush` are the only persistence calls. Adapter code is not given a direct database API. |
| Tests | `tests/experience_tests.cpp` covers weights/separation, decay, stale rounds, deterministic snapshots, restart round-trip, checksum corruption, backup recovery, schema mismatch, and map mismatch quarantine. |

## Verification

Focused portable x86 Debug verification during implementation:

```text
cmake --build build-portable-x86-test --target astrabot_experience_tests
ctest --test-dir build-portable-x86-test -R '^astrabot\.experience$' --output-on-failure
```

The canonical phase gate is run once at the P9 completion point:

```powershell
tools/verify-canonical.ps1 -Profile All
```

Live HLDS/ReHLDS and real-device validation remain outside this offline gate
and the project-wide Finish state.

Canonical verification completed once on the same P9 implementation tree:

```text
tools/verify-canonical.ps1 -Profile All
PortableDebug: PASS
MetamodDebug: PASS
MetamodRelease/export: PASS
```

Phase 9 Offline: PASS
