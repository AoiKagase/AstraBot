# P3-01 real NAV compatibility follow-up

Date: 2026-09-05. Base/main integration: `2700f1c`.
Branch: `codex/p301-real-nav-compatibility`.
Result: **Partially validated** for the two named local v5 files below.
The zero-ID load blocker is resolved; generator provenance and remaining
record-detail comparisons are still pending. P3-01 and project-wide Finish
are not complete. No Linux or live-server validation was performed.

## Integration and correction

The inspector commit was fast-forwarded into main, preserving `.focalspan.json`,
`.serena/` and existing worktrees. Main passed 18/18 Windows Debug tests (6.12 s)
with the inspector enabled; its FocalSpan index was updated. This follow-up
was implemented separately from that integrated main.

Only three validator contracts changed:

- HidingSpotId zero is a present ID; uniqueness still applies, including zero.
- EncounterSpotId zero resolves to that ID, or reports DanglingReference if absent.
- Approach here/previous/next zero means no reference; nonzero IDs must resolve.

Area IDs, connection IDs, encounter area IDs, duplicate detection, geometry,
EOF and all resource limits retain their existing rules. Public layouts and
wire formats are unchanged. `optional<uint32_t>{0}` is distinct from the absent
ID in v1. There is no new native format or generator in this change.

The pinned ReGameDLL reference is `b0889847fe6d03898be88acc9e366660efb40ab5`:

- [nav_area.cpp:75-151](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_area.cpp#L75-L151): reset next hiding ID to zero, allocate by increment, serialize/load raw ID and look up zero normally.
- [nav_file.cpp:487-549](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_file.cpp#L487-L549): nullable approach references, required encounter area references and hiding-ID lookup.

The sibling checkout has a different SHA and was not substituted for this pin.
The exact public pinned files were read during planning. They are format
evidence only; no upstream implementation was copied or translated.

## Independent comparisons

`tools/check-real-nav.ps1` is an original, opt-in v5 checker using .NET
BinaryReader, direct geometry arithmetic and a linear-scan Dijkstra oracle.
It does not call AstraBot to derive expectations. It compares actual inspector
reports to those expectations, and checks input size/SHA-256 before and after.
Its normal CTest exercise uses only original synthetic files, not game assets.

| File | Areas | Directed connections | Hiding spots | Approaches | Encounters | Encounter spots |
|---|---:|---:|---:|---:|---:|---:|
| de_dust.nav | 805 | 3172 | 328 | 6684 | 12918 | 25852 |
| de_dust2.nav | 737 | 2876 | 357 | 4803 | 12520 | 29215 |

Both files: magic FEEDFACE, version 5, zero Places, exact EOF, no duplicate
hiding IDs or unresolved references. Each has one HidingSpotId zero.
Approach zero-reference counts are 15 and 10 respectively. Header BSP lengths
match the supplied BSP files. Inspector metadata agrees with independent counts.

| Query | Ordered route | Inspector / independent cost |
|---|---|---:|
| dust 1 to 209 | 1,209 | 388.30737899680236 |
| dust 209 to 1 | 209,1 | 388.30737899680236 |
| dust2 1 to 1673 | 1,1673 | 388.90905168288754 |
| dust2 1673 to 1 | 1673,1 | 388.90905168288754 |

All four routes are Complete. Every selected direction/source/target is checked
against independently decoded edges; each edge and route component cost is
checked against geometry/Dijkstra, rather than requiring an arbitrary equal-cost
path. Inputs are float-round-tripped area centers with radius/vertical bounds 1.
Both containing matches and projections/distances agree with independent arithmetic.

- dust centers: `(-25,3300,0.031252555549144745)` and
  `(-237.5,2975,1.2019717693328857)`.
- dust2 centers: `(-1512.5,-612.5,128.03125)` and
  `(-1787.5,-887.5,128.53173828125)`.
- Forced nearestGeometry fallback: dust `(2575.5,587.5,0.03125397115945816)`
  resolves to area 1730; dust2 `(1800.5,562.5,80.03125)` to area 535.
  Both distances squared are 0.25; projections and same-area routes agree.

These are geometric/graph observations, not grounded movement acceptance.

## Inputs and reproduction

The user authorized local dust/dust2 use. Assets and private absolute paths are
not committed. All four sizes and hashes remained identical after inspection:

| File | Bytes | SHA-256 |
|---|---:|---|
| de_dust.nav | 432321 | `3a7fc7984876d9aa21f8e0d4d16fbfc51a2c40c13eed1d981ddd6bce24a3d688` |
| de_dust.bsp | 1359684 | `ac94fbda94e4a170b971f05936fe8706cb9aa895e55f7797916dc160171cab11` |
| de_dust2.nav | 413585 | `53b9889c0a5b45da7c1284b13db7d7b1218c185ea768dc261ce4c4d6e13372c2` |
| de_dust2.bsp | 2057288 | `15945389528d113562ede0a2c80647ebfa799079ed1c05a25379bcf84e4e9286` |

Use the inspector-enabled Windows x64 NMake Debug build from the inspector
report, then run with operator-supplied local paths:

```powershell
rtk proxy powershell -NoProfile -File tools/verify-nav-evidence.ps1 -Mode Debug
rtk proxy powershell -NoProfile -File tools/check-real-nav.ps1 -NavPath <de_dust.nav> -BspPath <de_dust.bsp> -Inspector build-portable-test/astrabot_nav_inspect.exe -GoalArea 209 -OutputDirectory build-portable-test/real-nav
rtk proxy powershell -NoProfile -File tools/check-real-nav.ps1 -NavPath <de_dust2.nav> -BspPath <de_dust2.bsp> -Inspector build-portable-test/astrabot_nav_inspect.exe -GoalArea 1673 -OutputDirectory build-portable-test/real-nav
```

Six real-file reports are retained in the ignored worktree build directory.
The checker emits a JSON summary with counts, hashes, exact positions and routes.

## Regression evidence and remaining work

- Windows x64 NMake Debug `/W4 /WX`: 19/19 CTest passed. All ten fixture hashes
  and the existing manifest regression checks passed.
- New v1-v5 cases retain absent versus zero hiding IDs, nullable approach fields,
  valid/missing zero encounter references and duplicate zero errors with exact
  diagnostic offsets. Existing nonzero missing references and invalid area/
  connection/encounter endpoints remain covered.
- RED observed: v1 zero ApproachAreaId rejected at offset 95. The test was
  changed to emit a console failure to avoid the Windows CRT assertion dialog;
  the semantic rejection was then captured before production edits.
- Corruption rejections are 2457 rather than historical 2465: eight blanket
  zero-HidingSpotId rejections (two per v2-v5 fixture) no longer apply. Positive,
  duplicate and unresolved-reference cases replace that invalid assumption.
  Approach corruption now uses missing nonzero ID 999. Fixture bytes/hashes and
  every allocation/count cap are unchanged; fuzz invariants use the same rules.
- The independent checker has synthetic pass and rejection coverage for missing
  goal, unsupported version and mismatching BSP length. No game file is required
  for CTest.
- Remaining: generator/date/32-bit writer provenance, independent sampled raw
  hiding/approach/encounter detail comparison, real files for other versions and
  post-Finish live acceptance. BSP size equality is not a cryptographic binding.
  The inspector's own NotYetValidated marker is intentionally unchanged: only
  this combined independent evidence supports the scoped Partially validated result.

The next independent P3-01 implementation slice is portable route-session and
snapshot/query contracts; console integration remains subsequent work.
